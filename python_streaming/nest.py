#!/usr/bin/env python2
# encoding: utf-8
# sample by andrewsosa001 : https://github.com/andrewsosa001/firebase-python-streaming
# changed by chaeplin

from firebase_streaming import Firebase
import json
import paho.mqtt.publish as mqtt
import socket
import ssl
import yaml

config = yaml.load(open('conf/nest.conf', 'r'))

structures_id  = config.get('structures_id')
device_id      = config.get('device_id')
access_token   = config.get('access_token')
child_path     = "/?auth=" + access_token

# add server CN to host file
mqtt_ca        = config.get('mqtt_ca')
mqtt_cn        = config.get('mqtt_cn')
mqtt_user      = config.get('mqtt_user')
mqtt_pass      = config.get('mqtt_pass')
mqtt_clientid  = config.get('mqtt_clientid')

topic_ac_set   = config.get('topic_ac_set')
first          = True

def publish(y):
    print y
    auth_user = {'username':mqtt_user, 'password':mqtt_pass}
    tls_user  = {'ca_certs':mqtt_ca, 'tls_version':ssl.PROTOCOL_TLSv1}
    mqtt.single(topic_ac_set, y, 0, 1, hostname=mqtt_cn, port=8883, client_id=mqtt_clientid, auth=auth_user, tls=tls_user)

def p(x):

    global first
    global is_online_prev
    global hvac_state_prev
    global target_temperature_c_prev
    global away_state_prev

    result = json.loads(x[1])

    if len(result['data']) == 3:
        print ("------------------------------------")
        print ("---> humidity : %s") % result['data']['devices']['thermostats'][device_id]['humidity']
        print ("---> hvac_mode : %s") % result['data']['devices']['thermostats'][device_id]['hvac_mode']
        print ("---> target_temperature_c : %s") % result['data']['devices']['thermostats'][device_id]['target_temperature_c']
        print ("---> ambient_temperature_c : %s") % result['data']['devices']['thermostats'][device_id]['ambient_temperature_c']
        print ("---> is_online : %s") % result['data']['devices']['thermostats'][device_id]['is_online']
        print ("---> hvac_state : %s") % result['data']['devices']['thermostats'][device_id]['hvac_state']
        print ("---> away_state : %s") % result['data']['structures'][structures_id]['away']
    
        is_online            = result['data']['devices']['thermostats'][device_id]['is_online']
        hvac_state           = result['data']['devices']['thermostats'][device_id]['hvac_state']
        target_temperature_c = result['data']['devices']['thermostats'][device_id]['target_temperature_c']
    
        if result['data']['structures'][structures_id]['away'] == 'home':
            away_state = '0'
        else:
            away_state = '1'
        
        if is_online == True:
            if first:
                if hvac_state == "off":
                    msg_str_set = "{\"ac_cmd\":0,"
                elif hvac_state == "cooling":
                    msg_str_set = "{\"ac_cmd\":1,"
    
                msg_str_set = msg_str_set + "\"ac_temp\":" + str(int(target_temperature_c)) + ",\"ac_flow\":1,\"away\":" + away_state + "}"
    
                publish(msg_str_set)
    
                is_online_prev              = is_online
                hvac_state_prev             = hvac_state
                target_temperature_c_prev   = target_temperature_c
                away_state_prev             = away_state
                first                       = False
    
            else:
                if target_temperature_c_prev != target_temperature_c or hvac_state_prev != hvac_state or away_state_prev != away_state:
                    if hvac_state == "off":
                        msg_str_set = "{\"ac_cmd\":0,"
                    elif hvac_state == "cooling":
                        msg_str_set = "{\"ac_cmd\":1,"
    
                    msg_str_set = msg_str_set + "\"ac_temp\":" + str(int(target_temperature_c)) + ",\"ac_flow\":1,\"away\":" + away_state + "}"
    
                    publish(msg_str_set)
    
                is_online_prev              = is_online
                hvac_state_prev             = hvac_state
                target_temperature_c_prev   = target_temperature_c
                away_state_prev             = away_state

# Firebase object
fb = Firebase('https://developer-api.nest.com/')

# Or use a custom callback
custom_callback = fb.child(child_path).listener(p)

try:
    custom_callback.start()

except KeyboardInterrupt:
    custom_callback.stop()
    sys.exit(1)
