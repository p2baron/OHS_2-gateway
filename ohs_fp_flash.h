/*
 * ohs_fp_flash.h — Gateway fingerprint template flash storage (sector 8)
 *
 * STM32F437xG is single-bank: CPU cannot fetch from flash during erase/write.
 * fpFlashErase and fpFlashWriteWord are placed in .data so they run from RAM.
 *
 * Layout: 20 fixed 1600-byte slots starting at 0x08080000.
 * Each slot: fp_slot_hdr_t (8 bytes) + compressed template data.
 *
 * fpBuf[] mirrors the flash sector in RAM. Populate at boot with memcpy from
 * FP_FLASH_BASE. All reads go to fpBuf; writes go to both fpBuf and flash
 * (via fpFlashWriteAll, called with chSysLock held).
 */

#ifndef OHS_FP_FLASH_H_
#define OHS_FP_FLASH_H_

#include "hal.h"
#include <string.h>
#include <stdbool.h>
#include "ohs_conf.h"

#define FP_FLASH_BASE   0x08080000U
#define FP_FLASH_SECT   8               // SNB field value for sector 8
#define FP_SLOT_SIZE    1600            // bytes per slot (header + compressed data)
#define FP_MAGIC        0xFE5A          // validity marker

typedef struct {
  uint16_t magic;   // FP_MAGIC when valid
  uint16_t id;      // gateway-assigned sequential ID (1-based, 0 = invalid)
  uint16_t size;    // compressed data length following this header
  uint16_t pad;     // padding to align to 8 bytes
} fp_slot_hdr_t;    // 8 bytes

static uint8_t  fpBuf[FINGERS_SIZE * FP_SLOT_SIZE];  // 32 KB RAM mirror
static uint16_t fpNextId = 1;                         // incremented at each enrollment
static bool     fpBackupDirty = false;                // fpBuf has unwritten changes

/* Must execute from RAM — do not call directly, use fpFlashWriteAll. */
__attribute__((noinline, section(".data")))
static void fpFlashErase(void) {
  while (FLASH->SR & FLASH_SR_BSY);
  FLASH->CR &= ~(FLASH_CR_PSIZE_Msk | FLASH_CR_SNB_Msk);
  FLASH->CR |= FLASH_CR_PSIZE_1                           // 32-bit parallelism
             | (FP_FLASH_SECT << FLASH_CR_SNB_Pos)
             | FLASH_CR_SER;
  FLASH->CR |= FLASH_CR_STRT;
  while (FLASH->SR & FLASH_SR_BSY);
  FLASH->CR &= ~(FLASH_CR_SER | FLASH_CR_SNB_Msk);
}

__attribute__((noinline, section(".data")))
static void fpFlashWriteWord(uint32_t addr, uint32_t word) {
  while (FLASH->SR & FLASH_SR_BSY);
  FLASH->CR |= FLASH_CR_PSIZE_1 | FLASH_CR_PG;
  *(volatile uint32_t *)addr = word;
  while (FLASH->SR & FLASH_SR_BSY);
  FLASH->CR &= ~FLASH_CR_PG;
}

/*
 * Erase sector 8 and write all FINGERS_SIZE slots from buf[].
 * MUST be called with interrupts locked (chSysLock / chSysUnlock at call site).
 * Takes ~2 s due to sector erase.
 */
static void fpFlashWriteAll(const uint8_t *buf) {
  uint32_t totalBytes = (uint32_t)(FINGERS_SIZE * FP_SLOT_SIZE);
  FLASH->KEYR = 0x45670123U;
  FLASH->KEYR = 0xCDEF89ABU;
  fpFlashErase();
  for (uint32_t i = 0; i < totalBytes; i += 4) {
    uint32_t word;
    memcpy(&word, buf + i, 4);
    fpFlashWriteWord(FP_FLASH_BASE + i, word);
  }
  FLASH->CR |= FLASH_CR_LOCK;
}

/*
 * Read compressed data for slot into dst[].
 * Returns compressed size, or 0 if slot is invalid.
 * dst must not be NULL.
 */
static uint16_t fpFlashRead(uint8_t slot, uint8_t *dst) {
  if (slot >= FINGERS_SIZE) return 0;
  fp_slot_hdr_t hdr;
  uint32_t off = (uint32_t)slot * FP_SLOT_SIZE;
  memcpy(&hdr, &fpBuf[off], sizeof(hdr));
  if (hdr.magic != FP_MAGIC || hdr.size == 0 || hdr.size > (FP_SLOT_SIZE - sizeof(fp_slot_hdr_t)))
    return 0;
  memcpy(dst, &fpBuf[off + sizeof(fp_slot_hdr_t)], hdr.size);
  return hdr.size;
}

/* Returns true if slot has a valid template (header only — no data copy). */
static bool fpFlashHasSlot(uint8_t slot) {
  if (slot >= FINGERS_SIZE) return false;
  fp_slot_hdr_t hdr;
  memcpy(&hdr, &fpBuf[(uint32_t)slot * FP_SLOT_SIZE], sizeof(hdr));
  return hdr.magic == FP_MAGIC && hdr.size > 0;
}

/* Returns the ID stored for a slot, or 0 if invalid. */
static uint16_t fpFlashGetId(uint8_t slot) {
  if (slot >= FINGERS_SIZE) return 0;
  fp_slot_hdr_t hdr;
  memcpy(&hdr, &fpBuf[(uint32_t)slot * FP_SLOT_SIZE], sizeof(hdr));
  return (hdr.magic == FP_MAGIC) ? hdr.id : 0;
}

/* Clear a slot in fpBuf (zero the magic). Call fpFlashWriteAll to persist. */
static void fpBufClearSlot(uint8_t slot) {
  if (slot >= FINGERS_SIZE) return;
  memset(&fpBuf[(uint32_t)slot * FP_SLOT_SIZE], 0, sizeof(fp_slot_hdr_t));
  fpBackupDirty = true;
}

/*
 * Async multipart send state — written by RS485 thread and HTTP thread,
 * consumed by SendThread. All outgoing RS485 sends go through SendThread.
 */

/* Resync: push all valid GW slots to one node (HTTP Resync button or mismatch on 'F'+'I'). */
static volatile bool    fpResyncPending = false;
static volatile uint8_t fpSyncAddr     = 0;

/* Distribute: push one newly enrolled template to all other FP nodes. */
static volatile bool     fpDistPending  = false;
static volatile uint8_t  fpDistSlot    = 0;
static volatile uint16_t fpDistId      = 0;
static volatile uint8_t  fpDistFromAddr = 0; // skip this address (the enrolling node)

#endif /* OHS_FP_FLASH_H_ */
