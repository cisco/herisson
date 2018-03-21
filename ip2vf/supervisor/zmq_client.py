#!/usr/bin/env python

import zmq
import sys
import requests
import socket

if len(sys.argv) < 4:
    print "Usage: %s <zmq_port> <http_serv> <local_ip> <remote_ip> "
    exit()

# retreive parameters
http_serv = sys.argv[2]
local_ip = sys.argv[3]
remote_ip = sys.argv[4]

def test_if_available(req):
    found = False
    try:
        print "test_if_available(): req="+req
        r = requests.get(req)
        print "r.status_code="+repr(r.status_code)+" for req="+req
        if r.status_code != 404:
            found = True
    except requests.exceptions.RequestException as e:    
        print "exception="+repr(e)
        pass
        
    return found

ctx = zmq.Context()
sock = ctx.socket(zmq.SUB)
subaddresse = "tcp://127.0.0.1:%s" % (sys.argv[1])
#sock.connect(subaddresse)
print "Bind on '" + subaddresse +"'"
sock.bind(subaddresse)
sock.setsockopt(zmq.SUBSCRIBE, "IP2VF")

while True:

    #
    # Recv msg from ZMQ
    #
    fullmsg = sock.recv()
    #print 'recv '+ fullmsg

    msg = fullmsg.split()[0]
    content = fullmsg.split()[1]
    
    #
    # Construct the request
    #
    id = -1
    base_req = "http://%s/" % (http_serv)
    if msg=="IP2VFINFOS":
        req = base_req + "moduleinfos"
    elif msg=="IP2VFSTATS":
        req = base_req + "modulestats"
    else:
        print "*E*: Error, invalid msg: '"+msg+"'"
        continue
        
    print '... ['+msg+'] content='+content
    for token in content.split(';'):
        param = token[:3]
        value = token[4:]
        if param=="ID_":
            id = value
        req = req + "/" + param + "/" +  value;
        
    #
    # For IP2VFINFOS msgs only, add some additional parameters, as IP and THUMB
    #
    print "msg==IP2VFINFOS = "+repr((msg=="IP2VFINFOS"))+" id="+repr(id)
    if msg=="IP2VFINFOS" and id > -1:
        # Add IP
        req += "/IP/" + local_ip
        # Add THUMB. Note, use local_ip to test if thumb exists, but remote_ip must be used by external application
        thumb_path = '/ip2vf3/'+id+'_frame.png'
        thumb_req = 'http://'+local_ip+":8080"+thumb_path
        thumb_found = test_if_available(thumb_req)
        if thumb_found == True:
            thumb_remote_path = remote_ip+thumb_path
            thumb_remote_path = thumb_remote_path.replace("/","-")
            thumb_remote_path = thumb_remote_path.replace(":","--")
            req += '/THUMB/'+thumb_remote_path

    print '... req='+req
    
    #
    # Send the request
    #
    try:
        r = requests.post(req)
    except requests.exceptions.RequestException as e:    
        #print e
        print "*E*: can't send the request to "+base_req
        pass
        
    sys.stdout.flush()

