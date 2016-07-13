#!/usr/bin/env python2
# encoding: utf-8
# sample by andrewsosa001 : https://github.com/andrewsosa001/firebase-python-streaming
# changed by chaeplin

from firebase_streaming import Firebase
import json

device_id    = "xxxxxxxxxxxx"
access_token = "xxxxxxxxxx"

#child_path   = "devices/thermostats/" + device_id + "?auth=" + access_token
child_path   = "devices/thermostats?auth=" + access_token

# Sample callback function
def p(x):
    result = json.loads(x[1])
    print json.dumps(result, sort_keys=True, indent=4, separators=(',', ': '))

# Firebase object
fb = Firebase('https://developer-api.nest.com/')

# Or use a custom callback
custom_callback = fb.child(child_path).listener(p)

try:
    custom_callback.start()

except KeyboardInterrupt:
    custom_callback.stop()
    sys.exit(1)