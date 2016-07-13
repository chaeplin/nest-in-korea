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
path_devel = "/devices/thermostats?auth=" + d['access_token']
conn_devel = httplib.HTTPSConnection(devel_url)
conn_devel.request("GET", path_devel)
conn_devel_httpResponse = conn_devel.getresponse()
#if conn_devel_httpResponse.reason != 'OK':
#	sys.exit(conn_devel_httpResponse.reason)

#firebase
path_firebase = conn_devel_httpResponse.getheader('Location')
match = re.search('https:\/\/(.*):(.*)\/(.*)\/(.*)$', path_firebase)
if match:
	firebase_url  = match.group(1)
	firebase_port = match.group(2)
	firbase_path1 = match.group(3)
	firbase_path2 = match.group(4)
        firbase_path = "/" + firbase_path1 + "/" + firbase_path2
else:
	sys.exit("Can't get firebase url")

firebase_devel = httplib.HTTPSConnection(firebase_url, firebase_port)
firebase_devel.request("GET", firbase_path)
firebase_devel_httpResponse = firebase_devel.getresponse()
result_firebase = firebase_devel_httpResponse.read()
device_id_match = re.search('{\"(.*)\":{\"humidity', result_firebase)
if device_id_match:
	print ("\r\n ---> device id is: %s") % device_id_match.group(1)
	print ("\r\n");
else:
        sys.exit("Can't get deviceid")

print "--------------------------------------------------------------")
result = json.loads(result_firebase)
print json.dumps(result, sort_keys=True, indent=4, separators=(',', ': '))


# end
