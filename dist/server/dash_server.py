#!/usr/bin/env python
"""A simple HTTP server

To start the server:

    python delayserver.py

Ipython Run:
    import BaseHTTPServer
    from delayserver import MyHTTPRequestHandler
    http = BaseHTTPServer.HTTPServer( ('localhost', 8000),
        MyHTTPRequestHandler)
    http.serve_forever()

To test with browser : 'http://198.248.242.16:8005/mpd/index.html')

To retriev from Python2.7 Shell:
    import urllib2
     urllib2.urlopen(
            "http://198.248.242.16:8006/mpd/x4ukwHdACDw.mpd").read()
"""
import time
import BaseHTTPServer
import sys
import os

sys.path.append('..')
#HOSTNAME = 'localhost'
HOSTNAME = '198.248.242.16'
PORT_NUMBER = 8006
BLOCK_SIZE = 1024

# 10 kbps when size is in bytes
RATE = None

HTML_PAGES = ['index.html']
MPD_FILES = ['mpd/index.html', 'mpd/x4ukwHdACDw.mpd']

class MyHTTPRequestHandler(BaseHTTPServer.BaseHTTPRequestHandler):
    "HTTPHandler to serve the DASH video"
    def do_GET(self):
        "Function to handle the get message"
        request = self.path.strip("/").split('?')[0]
        shutdown = False
        if request in HTML_PAGES:
            write_method = normal_write
            kwargs = {}
            print "Received Request for HTML File %s" % (request)
        elif request in HTML_PAGES + MPD_FILES:
            write_method = normal_write
            kwargs = {}
            print "Received Request for MPD File %s" % (request)
        elif request.split('.')[-1] in ['m4f', 'mp4']:
            write_method = normal_write
            kwargs = {}
            print "Received Reuest for DASH Media File %s" % (request)
        else:
            self.send_error(404)
            return
        self.send_response( 200 )
        self.send_header('ContentType', 'text/plain;charset=utf-8')
        self.send_header('Content-Length', str(os.path.getsize(request)))
        self.send_header('Pragma', 'no-cache' )
        self.end_headers()
        duration = write_method(self.wfile, request, **kwargs)
        print 'Request took %f seconds' % (duration)
        if shutdown:
            self.server.shutdown()

def normal_write(output, request):
    "Function to write the video onto output stream"
    with open(request, 'r') as request_file:
        start_time = time.time()
        output.write(request_file.read())
        now = time.time()
        output.flush()
    return now - start_time

def slow_write(output, request, rate=None):
    """Function to write the video onto output stream with interruptions in
    the stream
    """
    with open(request, 'r') as request_file:
        start_time = time.time()
        data = request_file.read(BLOCK_SIZE)
        output.write(data)
        last_send = time.time()
        current_stream = len(data)
        while (data != ''):
            if rate:
                if send_rate(BLOCK_SIZE, last_send - time.time()) >  rate:
                    continue
            output.write(data)
            last_send = time.time()
            current_stream += len(data)
            data = request_file.read(BLOCK_SIZE)
        now = time.time()
        output.flush()
    print 'Served %d bytes of file: %s in %f seconds' % (
                    current_stream, request, now - start_time)
    return now - start_time

def send_rate(data_size, time_to_send_data):
    """ Method to calculate the rate at which the data is sent
        Data_size in byes
        The return value is in kbps
    """
    while True:
        try:
            rate = data_size * 8 / (time_to_send_data) / 1000
            break
        except ZeroDivisionError:
            continue
    return rate

def main(stop_after_flv=False):
    "Function to start server"
    http_server = BaseHTTPServer.HTTPServer((HOSTNAME, PORT_NUMBER),
                                            MyHTTPRequestHandler)
    http_server.stop_after_flv = stop_after_flv
    print " ".join(("Listening on ", HOSTNAME, " at Port ",
        str(PORT_NUMBER), " - press ctrl-c to stop"))
    http_server.serve_forever()

if __name__ == "__main__":
    sys.exit(main())

