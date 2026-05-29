/*
 * ohs_th_send.h
 *
 *  Created on: 26. 2. 2026
 *      Author: vysocan
 */

#ifndef OHS_TH_SEND_H_
#define OHS_TH_SEND_H_

#ifndef SEND_DEBUG
#define SEND_DEBUG 0
#endif

#if SEND_DEBUG
#define DBG_SEND(...) {chprintf(console, __VA_ARGS__);}
#else
#define DBG_SEND(...)
#endif

/*
 * Node command/data send thread
 * Serializes all outgoing sendCmd/sendData through a single worker.
 */
static THD_WORKING_AREA(waSendThread, 384);
static THD_FUNCTION(SendThread, arg) {
  chRegSetThreadName(arg);
  nodeCmdEvent_t *inMsg;
  msg_t msg;
  int8_t resp;
  static uint8_t fpMpBuf[6 + FP_SLOT_SIZE]; // header + compressed template

  while (true) {
    // Handle pending multipart sends before blocking on mailbox.
    if (fpDistPending) {
      fpDistPending = false;
      uint16_t dLen = fpFlashRead(fpDistSlot, &fpMpBuf[6]);
      if (dLen > 0) {
        fpMpBuf[0]='F'; fpMpBuf[1]='C';
        fpMpBuf[2]=fpDistSlot; fpMpBuf[3]=0;
        memcpy(&fpMpBuf[4], (const void *)&fpDistId, 2);
        for (uint8_t n = 0; n < NODE_SIZE; n++) {
          if (node[n].type == 'K' && node[n].function == 'f' &&
              node[n].address != 0 && node[n].address != fpDistFromAddr) {
            sendDataMultipart(node[n].address, fpMpBuf, dLen + 6);
            chThdSleepMilliseconds(200);
          }
        }
      }
    }
    if (fpResyncPending) {
      fpResyncPending = false;
      for (uint8_t s = 0; s < FINGERS_SIZE; s++) {
        fp_slot_hdr_t rsHdr;
        memcpy(&rsHdr, &fpBuf[(uint32_t)s * FP_SLOT_SIZE], sizeof(rsHdr));
        if (rsHdr.magic != FP_MAGIC) continue;
        fpMpBuf[0]='F'; fpMpBuf[1]='C'; fpMpBuf[2]=s; fpMpBuf[3]=0;
        memcpy(&fpMpBuf[4], &rsHdr.id, 2);
        memcpy(&fpMpBuf[6], &fpBuf[(uint32_t)s * FP_SLOT_SIZE + sizeof(rsHdr)], rsHdr.size);
        sendDataMultipart(fpSyncAddr, fpMpBuf, rsHdr.size + 6);
        chThdSleepMilliseconds(200);
      }
    }

    msg = chMBFetchTimeout(&node_cmd_mb, (msg_t*)&inMsg, TIME_MS2I(500));
    if (msg == MSG_OK) {
      if (inMsg->length == 0) {
        // CMD mode
        resp = sendCmdDirect(inMsg->address, inMsg->data[0]);
        DBG_SEND("SendTH CMD to %u, cmd %u, resp %d\r\n",
                 inMsg->address, inMsg->data[0], resp);
      } else {
        // DATA mode
        resp = sendDataDirect(inMsg->address, inMsg->data, inMsg->length);
        DBG_SEND("SendTH DATA to %u, len %u, resp %d\r\n",
                 inMsg->address, inMsg->length, resp);
      }

      // Post-send actions on success
      if (resp == 1) {
        if ((inMsg->flags & NODE_CMD_FLAG_UPDATE_NODE) &&
            (inMsg->nodeIndex != DUMMY_NO_VALUE)) {
          node[inMsg->nodeIndex].lastOK = getTimeUnixSec();
          node[inMsg->nodeIndex].value = inMsg->value;
        }
        if ((inMsg->flags & NODE_CMD_FLAG_MQTT_PUB) &&
            (inMsg->nodeIndex != DUMMY_NO_VALUE)) {
          if (GET_NODE_MQTT(node[inMsg->nodeIndex].setting))
            pushToMqtt(typeSensor, inMsg->nodeIndex, functionValue);
        }
        // Free queue on success
        /*
        if ((inMsg->flags & NODE_CMD_FLAG_QUEUE) &&
            (inMsg->nodeIndex != DUMMY_NO_VALUE) &&
            (node[inMsg->nodeIndex].queue != NULL)) {
          umm_free(node[inMsg->nodeIndex].queue);
          node[inMsg->nodeIndex].queue = NULL;
        }
        */
      } else {
        // Queue data for sleeping nodes on failure
        /*
        if ((inMsg->flags & NODE_CMD_FLAG_QUEUE) &&
            (inMsg->nodeIndex != DUMMY_NO_VALUE) &&
            (inMsg->length > 0)) {
          if (node[inMsg->nodeIndex].queue == NULL) {
            node[inMsg->nodeIndex].queue = umm_malloc(inMsg->length);
          }
          if (node[inMsg->nodeIndex].queue != NULL) {
            memcpy(node[inMsg->nodeIndex].queue, inMsg->data, inMsg->length);
          }
        }
        */
      }

      chPoolFree(&node_cmd_pool, inMsg);
    }
  }
}

#endif /* OHS_TH_SEND_H_ */
