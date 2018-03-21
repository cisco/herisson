from flask import Flask, abort, make_response, jsonify, request, current_app, g
import zmq
import time
from datetime import timedelta
from datetime import datetime
from functools import update_wrapper


app = Flask(__name__)

context = zmq.Context()

modules = {}

def get_module(id):
    key = "ID"+repr(id)
    module = modules.get(key, None)  
    if module != None:
        return module
    else:
        print "Can't find module with id="+repr(id)+", create a new one!"
        module = {'id': id, 'name': ''}
        modules[key] = module
        return module
        
    return None

def update_module_infos(id, name, thumbnail, mtn_port, starttime, pin_id, pin_type, pin_direction, pid_frmsize, ip):
    module = get_module(id)  
    if module != None:
        module['name'] = name
        module['thumb'] = thumbnail
        module['mtn_port'] = mtn_port
        module['lasttime'] = int(round(time.time() * 1000))
        module['starttime'] = int(starttime)
        module['pin'+str(pin_id)+'_type'] = pin_type
        module['pin'+str(pin_id)+'_direction'] = pin_direction
        module['pin'+str(pin_id)+'_frmsize'] = pid_frmsize
        module['ip'] = ip

def update_module_stats(id, fps, frames, cpu_user, cpu_kernel, mem):
    module = get_module(id)  
    if module != None:
        module['fps'] = fps
        module['frames'] = frames
        module['lasttime'] = int(round(time.time() * 1000))
        module['cpu_user'] = cpu_user
        module['cpu_kernel'] = cpu_kernel
        module['mem'] = mem

RESULT_OK 	 = { 'result' : 'OK', 'content' : '' }
RESULT_ERROR = { 'result' : 'ERROR', 'content' : '' }
def make_OK_response_with_content( content ) :
    response = RESULT_OK;
    response['content'] = content
    return make_response(jsonify(response), 200)
def make_ERROR_response_with_content( content ) :
    response = RESULT_ERROR;
    response['content'] = content
    return make_response(jsonify(response), 200)

def crossdomain(origin=None, methods=None, headers=None,
                max_age=21600, attach_to_all=True,
                automatic_options=True):
    if methods is not None:
        methods = ', '.join(sorted(x.upper() for x in methods))
    if headers is not None and not isinstance(headers, basestring):
        headers = ', '.join(x.upper() for x in headers)
    if not isinstance(origin, basestring):
        origin = ', '.join(origin)
    if isinstance(max_age, timedelta):
        max_age = max_age.total_seconds()

    def get_methods():
        if methods is not None:
            return methods

        options_resp = current_app.make_default_options_response()
        #print "headers=" + repr(options_resp.headers)
        if not 'allow' in options_resp.headers:
            return u'HEAD, OPTIONS, GET'
        else:
            return options_resp.headers['allow']

    def decorator(f):
        def wrapped_function(*args, **kwargs):
            if automatic_options and request.method == 'OPTIONS':
                resp = current_app.make_default_options_response()
            else:
                resp = make_response(f(*args, **kwargs))
            if not attach_to_all and request.method != 'OPTIONS':
                return resp

            h = resp.headers

            h['Access-Control-Allow-Origin'] = origin
            h['Access-Control-Allow-Methods'] = get_methods()
            h['Access-Control-Max-Age'] = str(max_age)
            h['Last-Modified'] = datetime.now()
            h['Cache-Control'] = 'no-store, no-cache, must-revalidate, post-check=0, pre-check=0, max-age=0'
            h['Pragma'] = 'no-cache'
            h['Expires'] = '-1'
            if headers is not None:
                h['Access-Control-Allow-Headers'] = headers
            return resp

        f.provide_automatic_options = False
        return update_wrapper(wrapped_function, f)

    return decorator

@app.route("/")
@crossdomain(origin='*')
def hello():
    return "Hello World!"

@app.route("/modules/list", methods = ['GET', 'OPTIONS'])
@crossdomain(origin='*')
def http_get_modules_list():
    array = []
    for key in modules.keys():
        module = modules[key]
        obj = {}
        obj['id'] = module['id']
        obj['name'] = module['name']
        array.append( obj )

    #array.append( { 'id': '1', 'name': 'IP2VF#1', 'fps': 50 } )
    #array.append( { 'id': '2', 'name': 'Upscale' } )
    #array.append( { 'id': '3', 'name': 'Logo' } )
    #array.append( { 'id': '4', 'name': 'IP2VF#2' } )
    return make_OK_response_with_content( array )

@app.route("/modules/<int:id>", methods = ['GET', 'OPTIONS'])
@crossdomain(origin='*')
def http_get_module(id):
    key = "ID"+repr(id)
    module = modules.get(key, None)
    if module != None:
        return make_OK_response_with_content( module )
        
    return make_ERROR_response_with_content( {'id': id, 'error': 'unknown module'} )

@app.route("/modules/<int:id>/start", methods = ['GET', 'OPTIONS'])
@crossdomain(origin='*')
def http_start_module(id):
    key = "ID"+repr(id)
    module = modules.get(key, None)
    if module != None:
        interface = "tcp://"+str(module['ip'])+":"+str(module['mtn_port'])
        req = str("start:")
        subscriber = context.socket( zmq.PAIR )
        subscriber.connect( interface )
        subscriber.send( req )
        subscriber.close()
        return make_OK_response_with_content( module )
        
    return make_ERROR_response_with_content( {'id': id, 'error': 'unknown module'} )

@app.route("/modules/<int:id>/stop", methods = ['GET', 'OPTIONS'])
@crossdomain(origin='*')
def http_stop_module(id):
    key = "ID"+repr(id)
    module = modules.get(key, None)
    if module != None:
        interface = "tcp://"+str(module['ip'])+":"+str(module['mtn_port'])
        print "stop_module(): interface=" + repr(interface)
        req = str("stop:")
        subscriber = context.socket( zmq.PAIR )
        subscriber.connect( interface )
        subscriber.send( req )
        subscriber.close()	
        return make_OK_response_with_content( module )
        
    return make_ERROR_response_with_content( {'id': id, 'state': 'unknown module'} )

@app.route("/moduleinfos/ID_/<int:id>/NAM/<name>/MTN/<int:mtn_port>/STA/<starttime>/PID/<int:pin_id>/PTY/<pin_type>/PDI/<int:pin_direction>/PFS/<int:pid_frmsize>/IP/<ip>", methods = ['POST'])
@crossdomain(origin='*')
def http_post_module_info_without_thumb(id, name, mtn_port, starttime, pin_id, pin_type, pin_direction, pid_frmsize, ip ):
    #print "http_post_module_info() id=" + repr(id)

    thumb_full_url = ''
    update_module_infos(id, name, thumb_full_url, mtn_port, starttime, pin_id, pin_type, pin_direction, pid_frmsize, ip)

    return ""

@app.route("/moduleinfos/ID_/<int:id>/NAM/<name>/MTN/<int:mtn_port>/STA/<starttime>/PID/<int:pin_id>/PTY/<pin_type>/PDI/<int:pin_direction>/PFS/<int:pid_frmsize>/IP/<ip>/THUMB/<thumb>", methods = ['POST'])
@crossdomain(origin='*')
def http_post_module_info(id, name, mtn_port, starttime, pin_id, pin_type, pin_direction, pid_frmsize, ip, thumb ):
    #print "http_post_module_info() id=" + repr(id)

    thumb_full_url = 'http://'+thumb.replace("--",":")
    thumb_full_url = thumb_full_url.replace("-","/")
    update_module_infos(id, name, thumb_full_url, mtn_port, starttime, pin_id, pin_type, pin_direction, pid_frmsize, ip)

    return ""


@app.route("/modulestats/ID_/<int:id>/NAM/<name>/FPS/<float:fps>/FRM/<int:frames>/USE/<int:cpu_user>/KER/<int:cpu_kernel>/MEM/<int:mem>", methods = ['POST'])
@crossdomain(origin='*')
def http_post_module_stats(id, name, fps, frames, cpu_user, cpu_kernel, mem ):
    #print "post_module_stats() id=" + repr(id)

    update_module_stats(id, fps, frames, cpu_user, cpu_kernel, mem)

    return ""

if __name__ == "__main__":
    app.run(debug=True, host='0.0.0.0', port=5002)
