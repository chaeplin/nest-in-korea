#!/usr/bin/env python2
# encoding: utf-8
# sample by andrewsosa001 : https://github.com/andrewsosa001/firebase-python-streaming
# changed by chaeplin

from firebase_streaming import Firebase
import json
import paho.mqtt.client as mqtt
import socket
import ssl

device_id    = "xxxxx"
access_token = "xxxxx"
child_path   = "devices/thermostats?auth=" + access_token

mqtt_user = "xxxxx"
mqtt_pass = "xxxxx"

topic_ac_set = "cool/LIVING_ROOM/set"
topic_ac_cmd = "cool/LIVING_ROOM/cmd"
first        = True

# Sample callback function
def p(x):

    global first
    global is_online_prev
    global hvac_state_prev
    global target_temperature_c_prev

    result = json.loads(x[1])
    print ("------------------------------------")
    print ("---> humidity : %s") % result['data'][device_id]['humidity']
    print ("---> hvac_mode : %s") % result['data'][device_id]['hvac_mode']
    print ("---> target_temperature_c : %s") % result['data'][device_id]['target_temperature_c']
    print ("---> ambient_temperature_c : %s") % result['data'][device_id]['ambient_temperature_c']
    print ("---> is_online : %s") % result['data'][device_id]['is_online']
    print ("---> hvac_state : %s") % result['data'][device_id]['hvac_state']

    is_online            = result['data'][device_id]['is_online']
    hvac_state           = result['data'][device_id]['hvac_state']
    target_temperature_c = result['data'][device_id]['target_temperature_c']
    
    if is_online == True:
        if first:

            mqttc = mqtt.Client("nest")
            mqttc.tls_set("ca.crt")
            mqttc.tls_insecure_set(True)
            mqttc.username_pw_set(mqtt_user, mqtt_pass)
            mqttc.connect("127.0.0.1", port=8883, keepalive=3)

            msg_str_set = "{\"ac_temp\":" + str(int(target_temperature_c)) + ",\"ac_flow\":0}"
            mqttc.publish(topic_ac_set, msg_str_set, 0, 1)
            mqttc.loop()

            if hvac_state == "off":
                msg_str_cmd = "{\"ac_cmd\":0}"
            elif hvac_state == "cooling":
                msg_str_cmd = "{\"ac_cmd\":1}"

            mqttc.publish(topic_ac_cmd, msg_str_cmd, 0, 0)
            mqttc.loop()
            mqttc.disconnect()

            is_online_prev              = is_online
            hvac_state_prev             = hvac_state
            target_temperature_c_prev   = target_temperature_c
            first                       = False

            print (msg_str_set)
            print (msg_str_cmd)

        else:
            if target_temperature_c_prev != target_temperature_c:
                mqttc = mqtt.Client("nest")
                mqttc.tls_set("ca.crt")
                mqttc.tls_insecure_set(True)
                mqttc.username_pw_set(mqtt_user, mqtt_pass)
                mqttc.connect("127.0.0.1", port=8883, keepalive=1)

                msg_str_set = "{\"ac_temp\":" + str(int(target_temperature_c)) + ",\"ac_flow\":0}"
                mqttc.publish(topic_ac_set, msg_str_set, 0, 1)
                mqttc.loop()
                mqttc.disconnect()

                print (msg_str_set)

            if hvac_state_prev != hvac_state:
                mqttc = mqtt.Client("nest")
                mqttc.tls_set("ca.crt")
                mqttc.tls_insecure_set(True) 
                mqttc.username_pw_set(mqtt_user, mqtt_pass)
                mqttc.connect("127.0.0.1", port=8883, keepalive=1)

                if (hvac_state == "off"):
                    msg_str_cmd = "{\"ac_cmd\":0}"
                elif (hvac_state == "cooling"):
                    msg_str_cmd = "{\"ac_cmd\":1}"

                mqttc.publish(topic_ac_cmd, msg_str_cmd, 0, 0)
                mqttc.loop()
                mqttc.disconnect()

                print (msg_str_cmd)

            is_online_prev              = is_online
            hvac_state_prev             = hvac_state
            target_temperature_c_prev   = target_temperature_c

# Firebase object
fb = Firebase('https://developer-api.nest.com/')

# Or use a custom callback
custom_callback = fb.child(child_path).listener(p)

try:
    custom_callback.start()

except KeyboardInterrupt:
    custom_callback.stop()
    sys.exit(1)
