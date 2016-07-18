#!/usr/bin/env python2
# encoding: utf-8

import sys
import time
import httplib
import json
import re

url = "api.home.nest.com"
devel_url = "developer-api.nest.com"

product_id = "xxxx-xxxx-xxx-xxx-xxxx"
product_secret ="xxxxxxx"

#Authorization
print ("\r\nopen following url using brower, click 'ACCEPT'\r\n")
print ("https://home.nest.com/login/oauth2?client_id=" + product_id + "&state=STATE\r\n")

input_var = raw_input("Enter PINCODE: ")
print ("you entered " + input_var) 

path = "/oauth2/access_token?client_id=" + product_id + "&code=" + input_var + "&client_secret=" + product_secret + "&grant_type=authorization_code"

conn = httplib.HTTPSConnection(url)
conn.request("POST", path)
httpResponse = conn.getresponse()
if httpResponse.reason != 'OK':
	sys.exit(httpResponse.reason)

#print(httpResponse.read())
d = json.loads(httpResponse.read())
if 'access_token' in d:
	print ("\r\n ---> access token is : %s") % d['access_token']
else:
	print ("error!!\r\n")
	sys.exit("Can't get access_token")

# get device id
path_devel = "/?auth=" + d['access_token']
conn_devel = httplib.HTTPSConnection(devel_url)
conn_devel.request("GET", path_devel)
conn_devel_httpResponse = conn_devel.getresponse()

#firebase
path_firebase = conn_devel_httpResponse.getheader('Location')
match = re.search('https:\/\/(.*):(.*)\/(.*)$', path_firebase)

if match:
        firebase_url  = match.group(1)
        firebase_port = match.group(2)
        firbase_path  = match.group(3)
else:
        sys.exit("Can't get firebase url")

firebase_devel = httplib.HTTPSConnection(firebase_url, firebase_port)
firebase_devel.request("GET", firbase_path)
firebase_devel_httpResponse = firebase_devel.getresponse()
result_firebase = firebase_devel_httpResponse.read()
result = json.loads(result_firebase)

if len(result["structures"]) == 1:
    for key in result["structures"].keys():
        print ("\r\n ---> structures id is: %s") % key

if len(result["devices"]["thermostats"]) == 1:
    for key in result["devices"]["thermostats"].keys():
        print ("\r\n ---> device id is: %s") % key
