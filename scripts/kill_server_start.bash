#!/bin/bash

sudo erl -noshell -s kill_server start -s init stop -pa ~/riak-erlang-client/ebin ~/riak-erlang-client/deps/*/ebin 
