/*
 * httpd_handler_finger.h
 *
 *  Created on: May 2026
 *      Author: vysocan
 */

#ifndef HTTPD_HANDLER_FINGER_H_
#define HTTPD_HANDLER_FINGER_H_


/*
 * @brief HTTP fingerprint page handler
 * @param chp Pointer to the output stream
 */
static void fs_open_custom_finger(BaseSequentialStream *chp) {
  // Summary table
  chprintf(chp, "%s#", HTML_tr_th);
  chprintf(chp, "%s%s", HTML_e_th_th, TEXT_Name);
  chprintf(chp, "%s%s", HTML_e_th_th, TEXT_User);
  chprintf(chp, "%s%s", HTML_e_th_th, TEXT_On);
  chprintf(chp, "%s%s", HTML_e_th_th, TEXT_Panic);
  chprintf(chp, "%s%s", HTML_e_th_th, TEXT_Admin);
  chprintf(chp, "%s%s%s\r\n", HTML_e_th_th, "Stored", HTML_e_th_e_tr);
  static const char * const handStr[]   = {"?", "L", "R"};
  static const char * const fingerStr[] = {"?", "Thumb", "Idx", "Mid", "Ring", "Little"};
  for (uint8_t i = 0; i < FINGERS_SIZE; i++) {
    chprintf(chp, "%s%u.%s", HTML_tr_td, i + 1, HTML_e_td_td);
    if (conf.finger[i].hand > 0 && conf.finger[i].fingerIdx > 0)
      chprintf(chp, "%s.%s", handStr[conf.finger[i].hand], fingerStr[conf.finger[i].fingerIdx]);
    else
      chprintf(chp, "%s", NOT_SET);
    chprintf(chp, "%s", HTML_e_td_td);
    if (conf.finger[i].contact == DUMMY_NO_VALUE) chprintf(chp, "%s", NOT_SET);
    else chprintf(chp, "%s", conf.contact[conf.finger[i].contact].name);
    chprintf(chp, "%s", HTML_e_td_td);
    printOkNok(chp, GET_CONF_FINGER_ENABLED(conf.finger[i].setting));
    chprintf(chp, "%s", HTML_e_td_td);
    printOkNok(chp, GET_CONF_FINGER_PANIC(conf.finger[i].setting));
    chprintf(chp, "%s", HTML_e_td_td);
    printOkNok(chp, GET_CONF_FINGER_ADMIN(conf.finger[i].setting));
    chprintf(chp, "%s", HTML_e_td_td);
    printOkNok(chp, fpFlashHasSlot(i));
    chprintf(chp, "%s", HTML_e_td_e_tr);
  }
  chprintf(chp, "%s", HTML_e_table);

  // Edit section
  chprintf(chp, "%s", HTML_table);
  chprintf(chp, "%s%s %s%s", HTML_tr_td, TEXT_Fingerprint, TEXT_Number, HTML_e_td_td);
  chprintf(chp, "%sP%s", HTML_select_submit, HTML_e_tag);
  for (uint8_t i = 0; i < FINGERS_SIZE; i++) {
    chprintf(chp, "%s%u", HTML_option, i);
    if (webFinger == i) chprintf(chp, "%s", HTML_selected);
    else                chprintf(chp, "%s", HTML_e_tag);
    chprintf(chp, "%u.%s", i + 1, HTML_e_option);
  }
  chprintf(chp, "%s%s", HTML_e_select, HTML_e_td_e_tr_tr_td);

  // Hand
  chprintf(chp, "Hand%s", HTML_e_td_td);
  chprintf(chp, "%sh%s", HTML_select_submit, HTML_e_tag);
  { static const char * const hLbl[] = {"Not set", "Left", "Right"};
    for (uint8_t i = 0; i < 3; i++) {
      chprintf(chp, "%s%u", HTML_option, i);
      if (conf.finger[webFinger].hand == i) chprintf(chp, "%s", HTML_selected);
      else                                  chprintf(chp, "%s", HTML_e_tag);
      chprintf(chp, "%s%s", hLbl[i], HTML_e_option);
    }
  }
  chprintf(chp, "%s%s", HTML_e_select, HTML_e_td_e_tr_tr_td);
  // Finger
  chprintf(chp, "Finger%s", HTML_e_td_td);
  chprintf(chp, "%sf%s", HTML_select_submit, HTML_e_tag);
  { static const char * const fLbl[] = {"Not set","Thumb","Index","Middle","Ring","Little"};
    for (uint8_t i = 0; i < 6; i++) {
      chprintf(chp, "%s%u", HTML_option, i);
      if (conf.finger[webFinger].fingerIdx == i) chprintf(chp, "%s", HTML_selected);
      else                                       chprintf(chp, "%s", HTML_e_tag);
      chprintf(chp, "%s%s", fLbl[i], HTML_e_option);
    }
  }
  chprintf(chp, "%s%s", HTML_e_select, HTML_e_td_e_tr_tr_td);

  // Contact (user)
  chprintf(chp, "%s%s", TEXT_User, HTML_e_td_td);
  chprintf(chp, "%sc%s", HTML_select, HTML_e_tag);
  for (uint8_t i = 0; i <= CONTACTS_SIZE; i++) {
    if (i < CONTACTS_SIZE) {
      chprintf(chp, "%s%u", HTML_option, i);
      if (conf.finger[webFinger].contact == i) chprintf(chp, "%s", HTML_selected);
      else                                     chprintf(chp, "%s", HTML_e_tag);
      chprintf(chp, "%u. %s%s", i + 1, conf.contact[i].name, HTML_e_option);
    } else {
      chprintf(chp, "%s%u", HTML_option, DUMMY_NO_VALUE);
      if (conf.finger[webFinger].contact == DUMMY_NO_VALUE) chprintf(chp, "%s", HTML_selected);
      else                                                  chprintf(chp, "%s", HTML_e_tag);
      chprintf(chp, "%s%s", NOT_SET, HTML_e_option);
    }
  }
  chprintf(chp, "%s%s", HTML_e_select, HTML_e_td_e_tr_tr_td);

  // Enabled
  chprintf(chp, "%s %s%s", TEXT_Fingerprint, TEXT_is, HTML_e_td_td);
  printOnOffButton(chp, "0", GET_CONF_FINGER_ENABLED(conf.finger[webFinger].setting));
  chprintf(chp, "%s%s%s", HTML_e_td_e_tr_tr_td, TEXT_Panic, HTML_e_td_td);
  printOnOffButton(chp, "1", GET_CONF_FINGER_PANIC(conf.finger[webFinger].setting));
  chprintf(chp, "%s%s%s", HTML_e_td_e_tr_tr_td, TEXT_Admin, HTML_e_td_td);
  printOnOffButton(chp, "2", GET_CONF_FINGER_ADMIN(conf.finger[webFinger].setting));
  chprintf(chp, "%s", HTML_e_td_e_tr_tr_td);

  // Target node for enrollment
  chprintf(chp, "%s%s", TEXT_Node, HTML_e_td_td);
  chprintf(chp, "%sN%s", HTML_select, HTML_e_tag);
  for (uint8_t i = 0; i < NODE_SIZE; i++) {
    if (node[i].address != 0 && node[i].type == 'K' && node[i].function == 'f') {
      chprintf(chp, "%s%u", HTML_option, i);
      if (webFingerNode == i) chprintf(chp, "%s", HTML_selected);
      else                    chprintf(chp, "%s", HTML_e_tag);
      chprintf(chp, "%u. %s%s", i + 1, node[i].name, HTML_e_option);
    }
  }
  chprintf(chp, "%s%s", HTML_e_select, HTML_e_td_e_tr);
  chprintf(chp, "%s", HTML_e_table);

  // Buttons
  chprintf(chp, "%s%s%s%s", HTML_Apply, HTML_Save, HTML_Enroll, HTML_Delete);
}

/*
 * @brief HTTP fingerprint POST handler
 * @param postDataP Pointer to POST data string
 */
static void httpd_post_custom_finger(char **postDataP) {
  uint16_t number, valueLen = 0;
  char name[3];
  bool repeat;
  char *valueP;
  uint8_t message[REG_PACKET_SIZE + 1];

  do {
    repeat = getPostData(postDataP, &name[0], sizeof(name), &valueP, &valueLen);
    DBG_HTTP("Parse: %s = '%.*s' (%u)\r\n", name, valueLen, valueP, valueLen);
    switch(name[0]) {
      case 'P': // slot select
        number = strtol(valueP, NULL, 10);
        if (number < FINGERS_SIZE && number != webFinger) {
          webFinger = (uint8_t)number;
          repeat = 0;
        }
        break;
      case 'h': // hand (0=not set, 1=left, 2=right)
        number = strtol(valueP, NULL, 10);
        if (number < 3) conf.finger[webFinger].hand = (uint8_t)number;
        break;
      case 'f': // finger index (0=not set, 1=thumb..5=little)
        number = strtol(valueP, NULL, 10);
        if (number < 6) conf.finger[webFinger].fingerIdx = (uint8_t)number;
        break;
      case 'c': // contact
        conf.finger[webFinger].contact = (uint8_t)strtol(valueP, NULL, 10);
        break;
      case 'N': // target node index for enrollment
        number = strtol(valueP, NULL, 10);
        if (number < NODE_SIZE) webFingerNode = (uint8_t)number;
        break;
      case '0' ... '7': // setting bits
        if (valueP[0] == '0') conf.finger[webFinger].setting &= ~(1 << (name[0] - '0'));
        else                  conf.finger[webFinger].setting |=  (1 << (name[0] - '0'));
        break;
      case 'E': // enroll — send 'F'+'E'+location to target node
        message[0] = 'F';
        message[1] = 'E';
        message[2] = (uint8_t)webFinger;
        pushNodeData(node[webFingerNode].address, message, 3,
                     DUMMY_NO_VALUE, 0, NODE_CMD_FLAG_NONE);
        break;
      case 'D': // delete from node, clear GW flash slot and conf metadata
        message[0] = 'F';
        message[1] = 'D';
        message[2] = (uint8_t)webFinger;
        pushNodeData(node[webFingerNode].address, message, 3,
                     DUMMY_NO_VALUE, 0, NODE_CMD_FLAG_NONE);
        fpBufClearSlot((uint8_t)webFinger);
        conf.finger[webFinger].hand      = 0;
        conf.finger[webFinger].fingerIdx = 0;
        conf.finger[webFinger].contact   = DUMMY_NO_VALUE;
        conf.finger[webFinger].setting   = 0;
        writeToBkpSRAM((uint8_t*)&conf, sizeof(config_t), 0);
        tmpLog[0]='K'; tmpLog[1]='D'; tmpLog[2]=(uint8_t)webFinger; tmpLog[3]=node[webFingerNode].address;
        pushToLog(tmpLog, 4);
        break;
      case 'e': // save to backup SRAM
        writeToBkpSRAM((uint8_t*)&conf, sizeof(config_t), 0);
        break;
    }
  } while (repeat);
}


#endif /* HTTPD_HANDLER_FINGER_H_ */
