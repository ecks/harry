#!/usr/bin/env python
"""
Very simple HTTP server in python.

Usage::
    ./dummy-web-server.py [<port>]

Send a GET request::
    curl http://localhost

Send a HEAD request::
    curl -I http://localhost

Send a POST request::
    curl -d "foo=bar&bin=baz" http://localhost

"""
from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer
import SocketServer
import json
from pprint import pprint
import subprocess
import os

appIdDict = {'sibling-0': 0, 'sibling-1': 0, 'sibling-2': 0,}

class S(BaseHTTPRequestHandler):
#    def _set_headers(self):
#        self.send_response(200)
#        self.send_header('Content-type', 'text/html')
#        self.end_headers()

#    def do_GET(self):
#        self._set_headers()
#        self.wfile.write("<html><body><h1>hi!</h1></body></html>")

#    def do_HEAD(self):
#        self._set_headers()
        
    def do_POST(self):
        # Doesn't do anything with posted data
#        self._set_headers()
#        self.wfile.write("<html><body><h1>POST!</h1></body></html>")
        req_data = self.rfile.read(int(self.headers.getheader('Content-Length')))
        json_data = json.loads(req_data)
        pprint(json_data)
        if "taskStatus" in json_data:
          task_status = json_data["taskStatus"]
          if task_status == "TASK_FAILED":
            appId = json_data["appId"]
            appIdParsed = appId[1:]
            if appIdDict[appIdParsed] == 0:
              subprocess.call(["./put.sh", appIdParsed])
              appIdDict[appIdParsed] = 1
              
        
def run(server_class=HTTPServer, handler_class=S, port=80):
    server_address = ('', port)
    httpd = server_class(server_address, handler_class)
    print 'Starting httpd...'
    os.chdir("/home/hasenov/zebralite/marathon/")
    httpd.serve_forever()

if __name__ == "__main__":
    from sys import argv

    if len(argv) == 2:
        run(port=int(argv[1]))
    else:
        run()
