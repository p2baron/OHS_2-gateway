/*
 * ohs_th_rs485.h
 *
 *  Created on: 22. 2. 2020
 *      Author: adam
 */

#ifndef OHS_TH_RS485_H_
#define OHS_TH_RS485_H_

#ifndef RS485_DEBUG
#define RS485_DEBUG 0
#endif

#if RS485_DEBUG
#define DBG_RS485(...) {chprintf(console, __VA_ARGS__);}
#else
#define DBG_RS485(...)
#endif

/*
 * RS485 thread
 */
static THD_WORKING_AREA(waRS485Thread, 320);
static THD_FUNCTION(RS485Thread, arg) {
  chRegSetThreadName(arg);
  event_listener_t serialListener;
  eventmask_t evt;
  msg_t resp;
  RS485Msg_t rs485Msg;
  uint8_t nodeIndex, index;
  time_t timeNow;

  // Register
  chEvtRegister((event_source_t *)&RS485D2.event, &serialListener, EVENT_MASK(0));

  while (true) {
    evt = chEvtWaitAny(ALL_EVENTS);
    (void)evt;

    eventflags_t flags = chEvtGetAndClearFlags(&serialListener);
    DBG_RS485("%u: ", chVTGetSystemTime());
    DBG_RS485("RS485 flag: %u, state: %u, length: %u\r\n", flags, RS485D2.trcState, RS485D2.ibHead);
    //resp = chBSemWait(&RS485D2.received);
    if ((flags & RS485_MSG_RECEIVED) ||
        (flags & RS485_MSG_RECEIVED_WA)){
      resp = rs485GetMsg(&RS485D2, &rs485Msg);

      DBG_RS485("RS485: %d, ", resp);
      DBG_RS485("from: %u, ", rs485Msg.address);
      if (rs485Msg.ctrl) {
        DBG_RS485("command: %u.", rs485Msg.length);
      } else {
        DBG_RS485("data (len: %u): ", rs485Msg.length);
        for(uint8_t i = 0; i < rs485Msg.length; i++) {DBG_RS485("%x, ", rs485Msg.data[i]);}
      }
      DBG_RS485("\r\n");
      //DBG_RS485("ib %d %d %d , ob %d %d %d\r\n", RS485D2.ib[0], RS485D2.ib[1],
      //          RS485D2.ib[2], RS485D2.ob[0], RS485D2.ob[1], RS485D2.ob[2]);

      if (resp == MSG_OK) {
        if (rs485Msg.ctrl == RS485_FLAG_CMD) {
          switch(rs485Msg.length) {
            case NODE_CMD_PING: // Nodes should do periodic ping to stay alive/registered
              timeNow = getTimeUnixSec();
              index = 0; // Just any temp variable
              // Search in nodes
              for (nodeIndex = 0; nodeIndex < NODE_SIZE; nodeIndex++) {
                if (node[nodeIndex].address == rs485Msg.address) {
                  node[nodeIndex].lastOK = timeNow;
                  index++;
                }
              }
              // Search in remote zones
              for (nodeIndex = HW_ZONES; nodeIndex < ALARM_ZONES; nodeIndex++) {
                if (conf.zoneAddress[nodeIndex - HW_ZONES] == rs485Msg.address) {
                  index++;
                }
              }
              // If not found, call this node/zone to register.
              if (index == 0) {
                pushNodeCmd(rs485Msg.address, NODE_CMD_REGISTRATION); // call this address to register
                DBG_RS485("Unregistered node ping\r\n");
              } else {
                DBG_RS485("Node %d ping\r\n", rs485Msg.address);
              }
            break;
          }
        }
        if (rs485Msg.ctrl == RS485_FLAG_DTA) {
          switch(rs485Msg.data[0]) {
            case 'R': // Registration
              index = 1; // Skip 'R'
              do {
                registrationEvent_t *outMsg = chPoolAlloc(&registration_pool);
                if (outMsg != NULL) {
                  // node setting
                  outMsg->address  = rs485Msg.address;
                  outMsg->type     = (char)rs485Msg.data[index];
                  outMsg->function = (char)rs485Msg.data[index+1];
                  outMsg->number   = rs485Msg.data[index+2];
                  outMsg->setting  = (rs485Msg.data[index+3] << 8) | (rs485Msg.data[index+4]);
                  memcpy(&outMsg->name[0], &rs485Msg.data[index+5], NAME_LENGTH);  // Copy string

                  msg_t msg = chMBPostTimeout(&registration_mb, (msg_t)outMsg, TIME_IMMEDIATE);
                  if (msg != MSG_OK) {
                    //chprintf(console, "R-MB full %d\r\n", temp);
                  }
                } else {
                  pushToLogText("FR"); // Registration queue is full
                }
                index += REG_PACKET_SIZE;
              } while (index < rs485Msg.length);
              break;
            case 'K': // iButtons keys
              nodeIndex = getNodeIndex(rs485Msg.address, rs485Msg.data[0],
                                       rs485Msg.data[1], rs485Msg.data[2] - (rs485Msg.data[2] % 2));
              DBG_RS485("Received Key, node index: %d\r\n", nodeIndex);
              // Node index found
              if (nodeIndex != DUMMY_NO_VALUE) {
                node[nodeIndex].lastOK = getTimeUnixSec(); // Update timestamp
                //  Node is enabled
                if (GET_NODE_ENABLED(node[nodeIndex].setting)) {
                  if (rs485Msg.data[1] == 'f') {
                    // Fingerprint auth — location in data[9..10]
                    uint16_t fingerId;
                    memcpy(&fingerId, &rs485Msg.data[9], 2);
                    node[nodeIndex].value = (float)checkFinger(
                        GET_NODE_GROUP(node[nodeIndex].setting),
                        (armType_t)(rs485Msg.data[2] % 2), fingerId);
                  } else {
                    node[nodeIndex].value = (float)checkKey(GET_NODE_GROUP(node[nodeIndex].setting),
                             (rs485Msg.data[2] % 2), &rs485Msg.data[3], rs485Msg.length - 4);
                  }
                  // MQTT
                  if (GET_NODE_MQTT(node[nodeIndex].setting)) {
                    pushToMqtt(typeSensor, nodeIndex, functionValue);
                  }
                } else {
                  // log disabled remote nodes
                  tmpLog[0] = 'N'; tmpLog[1] = 'F'; tmpLog[2] = rs485Msg.address;
                  tmpLog[3] = rs485Msg.data[0]; tmpLog[4] = rs485Msg.data[1];
                  tmpLog[5] = rs485Msg.data[2];  pushToLog(tmpLog, 6);
                }
              } else { // node not found
                // - send queue - chThdSleepMilliseconds(5);  // This is needed for sleeping battery nodes, or they wont see reg. command.
                pushNodeCmd(rs485Msg.address, NODE_CMD_REGISTRATION); // call this address to register
              }
              break;
            case 'S': // Sensor data
              index = 1; // skip 'S'
              do {
                sensorEvent_t *outMsg = chPoolAlloc(&sensor_pool);
                if (outMsg != NULL) {
                  // node setting
                  outMsg->address  = rs485Msg.address;
                  outMsg->function = (char)rs485Msg.data[index];
                  outMsg->number   = rs485Msg.data[index+1];
                  floatConv.byte[0] = rs485Msg.data[index+2]; floatConv.byte[1] = rs485Msg.data[index+3];
                  floatConv.byte[2] = rs485Msg.data[index+4]; floatConv.byte[3] = rs485Msg.data[index+5];
                  outMsg->value = floatConv.val;

                  msg_t msg = chMBPostTimeout(&sensor_mb, (msg_t)outMsg, TIME_IMMEDIATE);
                  if (msg != MSG_OK) {
                    //chprintf(console, "S-MB full %d\r\n", temp);
                  }
                } else {
                  pushToLogText("FS"); // Sensor queue is full
                }
                index += SENSOR_PACKET_SIZE;
              } while (index < rs485Msg.length);
              break;
            case 'Z': // Zone
              index = 1; // Skip 'Z'
              DBG_RS485("Zone");
              do {
                nodeIndex = rs485Msg.data[index] - 1; // Used here as temporary zone number
                DBG_RS485(": %d", nodeIndex);
                // Zone allowed
                if ((nodeIndex >= HW_ZONES) && (nodeIndex < ALARM_ZONES)) {
                  // Zone enabled
                  if (GET_CONF_ZONE_ENABLED(conf.zone[nodeIndex])) {
                    // Zone address and sender address match = zone is remote zone
                    if (conf.zoneAddress[nodeIndex-HW_ZONES] == (rs485Msg.address)){
                      DBG_RS485(" = %c", rs485Msg.data[index+1]);
                      // Check for valid lastEvent
                      if ((rs485Msg.data[index+1] == 'O') || (rs485Msg.data[index+1] == 'P') ||
                          (rs485Msg.data[index+1] == 'T')) {
                        zone[nodeIndex].lastEvent = rs485Msg.data[index+1];
                        if (rs485Msg.data[index+1] == 'O') {
                          zone[nodeIndex].lastOK = getTimeUnixSec();  // update current timestamp
                        } else {
                          zone[nodeIndex].lastPIR = getTimeUnixSec(); // update current timestamp
                        }
                      }
                    } else {
                      // Log error just once
                      if (!GET_ZONE_ERROR(zone[nodeIndex].setting)) {
                        tmpLog[0] = 'Z'; tmpLog[1] = 'e'; tmpLog[2] = nodeIndex; tmpLog[3] = 'M'; pushToLog(tmpLog, 4);
                        SET_ZONE_ERROR(zone[nodeIndex].setting); // Set error flag
                      }
                    }
                  } else {
                    // Log error just once
                    if (!GET_ZONE_ERROR(zone[nodeIndex].setting)) {
                      tmpLog[0] = 'Z'; tmpLog[1] = 'e'; tmpLog[2] = nodeIndex; tmpLog[3] = 'N'; pushToLog(tmpLog, 4);
                      SET_ZONE_ERROR(zone[nodeIndex].setting); // Set error flag
                    }
                  } // else / Zone enabled
                } // Zone allowed
                index += ZONE_PACKET_SIZE;
              } while (index < rs485Msg.length);
              DBG_RS485("\r\n");
              break;
            case 'F': // Fingerprint single-packet messages from nodes
              if (rs485Msg.data[1] == 'I') {
                // ID table response from node after 'F'+'Q' query
                uint16_t nodeIds[FINGERS_SIZE];
                memcpy(nodeIds, &rs485Msg.data[2], FINGERS_SIZE * sizeof(uint16_t));
                bool mismatch = false;
                for (uint8_t s = 0; s < FINGERS_SIZE; s++) {
                  uint16_t gwId = fpFlashGetId(s);
                  if (gwId != nodeIds[s]) { mismatch = true; break; }
                }
                if (mismatch) {
                  DBG_RS485("FP: ID mismatch for addr %u, scheduling resync\r\n", rs485Msg.address);
                  fpSyncAddr      = rs485Msg.address;
                  fpResyncPending = true;
                  tmpLog[0]='K'; tmpLog[1]='S'; tmpLog[2]=rs485Msg.address;
                  pushToLog(tmpLog, 3);
                }
              }
              break;
            case MP_MARKER: { // Multipart message from node
              int8_t mpResp = multipartRxProcess(&mpRx, rs485Msg.address,
                                                  rs485Msg.data, rs485Msg.length);
              if (mpResp == 1) {
                DBG_RS485("MP: reassembled %u bytes, type '%c%c'\r\n",
                           mpRx.receivedLength, mpRx.data[0], mpRx.data[1]);
                if (mpRx.data[0] == 'F' && mpRx.data[1] == 'U') {
                  // Template uploaded from enrolling node — store and distribute
                  uint16_t slot   = mpRx.data[2];
                  uint16_t payLen = (uint16_t)(mpRx.receivedLength - 4);
                  uint16_t newId  = fpNextId++;
                  if (slot < FINGERS_SIZE && payLen > 0 &&
                      payLen <= (FP_SLOT_SIZE - (uint16_t)sizeof(fp_slot_hdr_t))) {
                    fp_slot_hdr_t hdr = { FP_MAGIC, newId, payLen, 0 };
                    uint32_t off = (uint32_t)slot * FP_SLOT_SIZE;
                    memcpy(&fpBuf[off],                    &hdr,          sizeof(hdr));
                    memcpy(&fpBuf[off + sizeof(hdr)], &mpRx.data[4], payLen);
                    fpBackupDirty   = true;
                    fpDistSlot      = (uint8_t)slot;
                    fpDistId        = newId;
                    fpDistFromAddr  = rs485Msg.address;
                    fpDistPending   = true; // SendThread will distribute to other FP nodes
                    tmpLog[0]='K'; tmpLog[1]='E'; tmpLog[2]=(uint8_t)slot; tmpLog[3]=rs485Msg.address;
                    pushToLog(tmpLog, 4);
                    DBG_RS485("FP: enrolled slot %u id %u from addr %u, distributing\r\n",
                               slot, newId, rs485Msg.address);
                  }
                }
                multipartRxReset(&mpRx);
              } else if (mpResp < 0) {
                DBG_RS485("MP: error\r\n");
              }
            } break;
          } // switch case
        } // data
      } // MSG_OK
    } // (flags & RS485_MSG_RECEIVED)
  }
}

#endif /* OHS_TH_RS485_H_ */
