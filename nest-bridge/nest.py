#!/usr/bin/env python2
# -*- coding: utf-8 -*-

from firebase_streaming import Firebase
import json
import socket
import sys
import yaml
import time

import paho.mqtt.client as mqtt
import ssl

def on_connect(client, userdata, flags, rc):
    print ("Connected with result code "+str(rc))
    client.subscribe(mqtt_subscribe + "setAway", 0)
    client.subscribe(mqtt_subscribe + "setTargetTemperature", 0)
    client.subscribe(mqtt_subscribe + "setTargetHeatingCoolingState", 0)
    client.subscribe(mqtt_subscribe + "setFanSpeed", 0)

def on_message(client, userdata, msg):
    global first_setAway
    global first_setTargetHeatingCoolingState
    global first_setTargetTemperature
    global ts

    print (msg.topic+" "+str(msg.payload))
    now = time.time()
    if (msg.topic == mqtt_subscribe + "setAway"):
        if (first_setAway == False):
            if ((now - ts) > 60):
                homeaway_set.put(dict_Away[str(msg.payload)])
                ts = time.time()
        first_setAway = False
    elif (msg.topic == mqtt_subscribe + "setTargetHeatingCoolingState"):
        if (first_setTargetHeatingCoolingState == False):
            if ((now - ts) > 60):
                thermostat_set.put(dict_Hvac_mode[str(msg.payload)])
                ts = time.time()
        first_setTargetHeatingCoolingState = False
    elif (msg.topic == mqtt_subscribe + "setTargetTemperature"):
        if (first_setTargetTemperature == False):
            if ((now - ts) > 60):
                thermostat_set.put({"target_temperature_c": float(msg.payload)})
                payload = "{\"ac_temp\":" + str(msg.payload) + "}"
                client.publish(mqtt_esp8266, payload, 0, 1)
                ts = time.time()
        first_setTargetTemperature = False
    elif (msg.topic == mqtt_subscribe + "setFanSpeed"):
        payload = "{\"ac_flow\":" + str(msg.payload) + "}"
        client.publish(mqtt_esp8266, payload, 0, 1) 
    else:
        print("should not")

    if ((now - ts) <= 60):
        if (first == False):
            print (now - ts)
            payload = "{\"Away\":" + away_state + ",\"TargetHeatingCoolingState\":" + hvac_target + ",\"CurrentTemperature\":" + str(ambient_temperature_c) + ",\"CurrentRelativeHumidity\":" + str(humidity) + ",\"TargetTemperature\":" + str(target_temperature_c) + ",\"CurrentHeatingCoolingState\":" + CurrentHeatingCoolingState + "}"
            client.publish(mqtt_publish, payload, 0, 1)

def p(x):
    global away_state
    global hvac_target
    global ambient_temperature_c
    global humidity 
    global target_temperature_c
    global CurrentHeatingCoolingState
    global first

    result = json.loads(x[1])

    if len(result['data']) == 3:
        print ("------------------------------------")
        print ("---> humidity : %s") % result['data']['devices']['thermostats'][device_id]['humidity']
        print ("---> hvac_mode : %s") % result['data']['devices']['thermostats'][device_id]['hvac_mode']
        print ("---> target_temperature_c : %s") % result['data']['devices']['thermostats'][device_id]['target_temperature_c']
        print ("---> ambient_temperature_c : %s") % result['data']['devices']['thermostats'][device_id]['ambient_temperature_c']
        print ("---> is_online : %s") % result['data']['devices']['thermostats'][device_id]['is_online']
        print ("---> hvac_state : %s") % result['data']['devices']['thermostats'][device_id]['hvac_state']
        print ("---> away_state : %s") % result['data']['structures'][structure_id]['away']    

        away                  = result['data']['structures'][structure_id]['away']
        target_temperature_c  = result['data']['devices']['thermostats'][device_id]['target_temperature_c'] 
        humidity              = result['data']['devices']['thermostats'][device_id]['humidity']
        ambient_temperature_c = result['data']['devices']['thermostats'][device_id]['ambient_temperature_c']
        hvac_mode             = result['data']['devices']['thermostats'][device_id]['hvac_mode']       
        hvac_state            = result['data']['devices']['thermostats'][device_id]['hvac_state']

        if away == 'home':
            away_state = '0'
        elif away == 'away':
            away_state = '1'
        elif away == 'auto-away':
            away_state = '1'
        else:
            print("should not")

        if hvac_mode == 'off':
            hvac_target = '0'
        elif hvac_mode == 'heat':
            hvac_target = '1'
        elif hvac_mode == 'cool':
            hvac_target = '2'
        elif hvac_mode == 'heat-cool':
            hvac_target = '3'
        else:
            print("should not")

        if hvac_state == 'off':
            CurrentHeatingCoolingState = '0'
        elif hvac_state== 'heating':
            CurrentHeatingCoolingState = '1'
        elif hvac_state == 'cooling':
            CurrentHeatingCoolingState = '2'
        else:
            print("should not")

        payload = "{\"Away\":" + away_state + ",\"TargetHeatingCoolingState\":" + hvac_target + ",\"CurrentTemperature\":" + str(ambient_temperature_c) + ",\"CurrentRelativeHumidity\":" + str(humidity) + ",\"TargetTemperature\":" + str(target_temperature_c) + ",\"CurrentHeatingCoolingState\":" + CurrentHeatingCoolingState + "}"
        client.publish(mqtt_publish, payload, 0, 1)
        
        if ( first == True):
            first = False

#---------
config = yaml.load(open('./conf/nest.conf', 'r'))

structure_id   = config.get('structure_id')
device_id      = config.get('device_id')
access_token   = config.get('access_token')

streaming_path   = "?auth=" + access_token
homeaway_path    = "structures/" + structure_id + "?auth=" + access_token
thermostat_path  = "devices/thermostats/" + device_id + "?auth=" + access_token

# add server CN to host file
mqtt_ca        = config.get('mqtt_ca')
mqtt_cn        = config.get('mqtt_cn')
mqtt_user      = config.get('mqtt_user')
mqtt_pass      = config.get('mqtt_pass')
mqtt_clientid  = config.get('mqtt_clientid')
mqtt_subscribe = config.get('mqtt_subscribe')
mqtt_publish   = config.get('mqtt_publish')
mqtt_esp8266   = config.get('mqtt_esp8266')

dict_Away = {
    '0': {"away": "home"},
    '1': {"away": "away"},
    'false': {"away": "home"},
    'True': {"away": "away"}
}

dict_Hvac_mode = {
    '0': {"hvac_mode": "off"},
    '1': {"hvac_mode": "heat"},
    '2': {"hvac_mode": "cool"},
    '3': {"hvac_mode": "heat-cool"}
}

first = True
first_setAway  = True
first_setTargetHeatingCoolingState = True
first_setTargetTemperature = True
ts = time.time()

client = mqtt.Client(client_id=mqtt_clientid)
client.on_connect = on_connect
client.on_message = on_message
client.username_pw_set(mqtt_user, mqtt_pass)
client.tls_set(mqtt_ca, tls_version=ssl.PROTOCOL_TLSv1)
client.connect(mqtt_cn, 8883, 60)
client.loop_start()

# Firebase object
fb = Firebase('https://developer-api.nest.com/')

# Or use a custom callback
custom_callback = fb.child(streaming_path).listener(p)
homeaway_set   = fb.child(homeaway_path)
thermostat_set = fb.child(thermostat_path)

try:
    custom_callback.start()

except KeyboardInterrupt:
    custom_callback.stop()
    sys.exit(1)
