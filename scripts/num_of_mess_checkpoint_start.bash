#!/bin/bash

sudo erl -noshell -s num_of_messages_checkpointed -s init stop -pa ~/riak-erlang-client/ebin ~/riak-erlang-client/deps/*/ebin 
