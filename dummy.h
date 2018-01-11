
    // WiFi.begin("UPC1372015", "enigmAH_rotation_13");

  EEPROM.begin(sizeof (UnitData));
  UnitData data;
  strcpy(data.SSID, "ESONO");
  strcpy(data.pass, "glascadensiv");
  data.volt400 = 3.08;
  data.volt1000 = 2.88;
  //strcpy(data.target, "192.168.1.91");

  // 192,168,0,14
  data.target1 = 192;
  data.target2 = 168;
  data.target3 = 1;
  data.target4 = 91;
  
  data.port = 8585;
  EEPROM.put(0, data);
  EEPROM.end();
