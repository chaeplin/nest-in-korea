한국에서 NEST로 가정용 에어콘과 가스 보일러 제어
======================================

# Contents
- SW
 - [mosquitto and easyrsa](#mosquitto)
 - [node-red](#node-red)

### mosquitto
 - https://mosquitto.org/2013/01/mosquitto-debian-repository/
 - apt-get install mosquitto mosquitto-clients python-mosquitto python3-mosquitto

#### build and manage a PKI CA
 - for mqtt tls(no client cert. username/password will be used)
 - for wifi wpa2 enterprise(esp8266 only, need freeradius)
 - tool : https://github.com/OpenVPN/easy-rsa
```
git clone https://github.com/OpenVPN/easy-rsa.git
cd easy-rsa/easyrsa3
./easyrsa init-pki
./easyrsa build-ca
./easyrsa gen-req server nopass
./easyrsa sign-req server server
mkdir -p /etc/mosquitto/certs
cp pki/ca.crt pki/private/server.key pki/issued/server.crt  /etc/mosquitto/certs/
cd /etc/mosquitto/certs
chown mosquitto.mosquitto ca.crt server.crt server.key
```

```
./easyrsa gen-req client nopass
./easyrsa sign-req client client
```

extra for openvpn
```
./easyrsa gen-dh
./easyrsa export-p12 client
openvpn --genkey --secret ta.key

```


### get SHA1 Fingerprint of server crt
```
openssl x509 -fingerprint -in server.crt | sed -e 's/:/ /g'
```

#### /etc/mosquitto/mosquitto.conf
```
pid_file /var/run/mosquitto.pid
persistence true
persistence_location /var/lib/mosquitto/
log_dest file /var/log/mosquitto/mosquitto.log
autosave_interval 1800
connection_messages true
log_dest stderr
log_dest topic
log_type error
log_type warning
log_type notice
log_type information
log_type all
log_type debug
log_timestamp true
password_file /etc/mosquitto/conf.d/jp.pw
acl_file /etc/mosquitto/conf.d/jp.acl
persistence_file mosquitto.db
persistent_client_expiration 1m
retained_persistence true
listener 8883
tls_version tlsv1
cafile /etc/mosquitto/certs/ca.crt
certfile /etc/mosquitto/certs/server.crt
keyfile /etc/mosquitto/certs/server.key
require_certificate false
allow_anonymous false
```

* add user
```
touch /etc/mosquitto/conf.d/jp.acl
echo 'test:test123' > /etc/mosquitto/conf.d/jp.pw 
mosquitto_passwd -U /etc/mosquitto/conf.d/jp.pw
chown mosquitto.mosquitto /etc/mosquitto/conf.d/jp.pw /etc/mosquitto/conf.d/jp.pw
```

 - auto start
```
systemctl enable mosquitto (using update-rc.d)
```

* https://alexander-rudde.com/2014/02/install-mosquitto-mqtt-broker-on-raspberry-pi-running-arch-linux/
* if locale has error
- /etc/default/locale 

```
LANG=en_US.UTF-8
LC_ALL="en_US.UTF-8"
LANGUAGE="en_US:en"
```

## node-red
* http://nodered.org/docs/getting-started/installation
* http://nodered.org/docs/security
* https://www.hardill.me.uk/wordpress/2015/05/11/securing-node-red/


