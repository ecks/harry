-module(num_of_messages_checkpointed).
-compile(export_all).

start() ->
  start_server(9998).

start_server(Port) ->
  {ok, Listen} = gen_tcp:listen(Port, [binary, {active, false}, {reuseaddr, true}]),
  {ok, Pid} = riakc_pb_socket:start_link("10.100.2.1", 8087),
  Comp = "0",
  acceptor(Listen, Pid, Comp),
  {ok}.

acceptor(ListenSocket, Pid, Comp) ->
  case gen_tcp:accept(ListenSocket) of
  {ok, Socket} ->
    {ok, Comp2} = handle(Socket, Pid, Comp),
    acceptor(ListenSocket, Pid, Comp2);
  _ ->
    acceptor(ListenSocket, Pid, Comp)
  end.

handle(Socket, Pid, Comp) ->
  inet:setopts(Socket, [{active, once}]),
  receive
    {tcp, Socket, <<"comp set ", Msg:1/binary, _/binary>>} ->
      io:format("~s ~n", [Msg]),
      Comp2 = binary_to_list(Msg),
      gen_tcp:close(Socket),
      {ok, Comp2};
    {tcp, Socket, <<"comp get", _/binary>>} ->
      io:format(" ~s ~n", ["comp get"]),
      gen_tcp:send(Socket, Comp),
      gen_tcp:close(Socket),
      {ok, Comp};
    {tcp, Socket, <<"num", _/binary>>} -> 
      {ok, Keys} = riakc_pb_socket:list_keys(Pid, <<"0">>),
      Keys_filtered = lists:filter( fun(KeyInList) -> KeyInList /= <<"cur_xid">> end, Keys),
      gen_tcp:send(Socket, integer_to_list(length(Keys_filtered))),
      gen_tcp:close(Socket),
      {ok, Comp}
  end.
