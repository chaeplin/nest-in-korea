#!/usr/bin/env python2
# encoding: utf-8

from firebase_streaming import Firebase
import json
import socket
import sys
import yaml

import paho.mqtt.client as mqtt
import ssl

def on_connect(client, userdata, flags, rc):
    print ("Connected with result code "+str(rc))
    client.subscribe(mqtt_subscribe + "setAway", 0)
    client.subscribe(mqtt_subscribe + "setTargetTemperature", 0)
    client.subscribe(mqtt_subscribe + "setTargetHeatingCoolingState", 0)

def on_message(client, userdata, msg):
    print (msg.topic+" "+str(msg.payload))
    if (msg.topic == mqtt_subscribe + "setAway"):
        homeaway_set.put(dict_Away[str(msg.payload)])

    if (msg.topic == mqtt_subscribe + "setTargetHeatingCoolingState"):
        thermostat_set.put(dict_Hvac_mode[str(msg.payload)])

    if (msg.topic == mqtt_subscribe + "setTargetTemperature"):
        thermostat_set.put({"target_temperature_c": float(msg.payload)})

def p(x):
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

#---------
config = yaml.load(open('conf/nest.conf', 'r'))

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

#setAway
#setTargetTemperature
#setTargetHeatingCoolingState
#
#isAway
#getTargetTemperature
#getTargetHeatingCoolingState
#getCurrentTemperature
#getCurrentRelativeHumidity
#getCurrentHeatingCoolingState
#

dict_Away = {
    '0': {"away": "home"},
    '1': {"away": "away"}
}

dict_Hvac_mode = {
    '0': {"hvac_mode": "off"},
    '1': {"hvac_mode": "heat"},
    '2': {"hvac_mode": "cool"},
    '3': {"hvac_mode": "heat-cool"}
}

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
homeaway_set    = fb.child(homeaway_path)
thermostat_set  = fb.child(thermostat_path)

try:
    custom_callback.start()

except KeyboardInterrupt:
    custom_callback.stop()
    sys.exit(1)