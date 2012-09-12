-module(zebralite_test).
-compile(export_all).

start_server(Port) ->
    spawn(fun() -> start_nano_server(Port),
                   lib_misc:sleep(infinity)
          end).

start_nano_server(Port) ->
    {ok, Listen} = gen_tcp:listen(Port, [binary, {packet, 0},  %% (6)
					 {reuseaddr, true},
					 {active, true}]),
    {ok, Socket} = gen_tcp:accept(Listen),  %% (7)
    gen_tcp:close(Listen),  %% (8)
    loop(Socket).

loop(Socket) ->
    HelloType = 0,
    FeatureRequestType = 5,
    Version = 1,
    Length = 8,
    Xid = 1,
    Hello = <<Version:8, HelloType:8, Length:16, Xid:32>>,
    FeatureRequest = <<Version:8, FeatureRequestType:8, Length:16, Xid:32>>,
    receive
	{tcp, Socket, Bin} ->
	    io:format("Server received binary = ~p~n",[Bin]),
	    gen_tcp:send(Socket, Hello),  %% (11)
            gen_tcp:send(Socket, FeatureRequest),
	    loop(Socket);
	{tcp_closed, Socket} ->
	    io:format("Server socket closed~n")
    end.
