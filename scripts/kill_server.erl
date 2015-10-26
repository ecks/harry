-module(kill_server).
-compile(export_all).

-record(sibling, {id, 
                  pid,
                  leader,
                  valid}).

new_sibling(Id, Pid, Leader) ->
  if Leader == "1" ->
      #sibling{id=Id,
             pid=Pid,
             leader=true,
              valid=true};
    true ->
      #sibling{id=Id,
             pid=Pid,
             leader=false,
              valid=true}
  end.

update_sibling(Sibling, Id, Pid) ->
  if Sibling#sibling.id == Id ->
       % want to make usre the PId we are updating is actually different from previous one
       if Sibling#sibling.pid /= Pid ->
         io:format("sibling updated, Pids are different, [Old: ~s, New: ~s]~n", [Sibling#sibling.pid, Pid]),
         #sibling{id=Id,
                  pid=Pid,
                  leader=Sibling#sibling.leader,
                  valid=true};  % whenever updating the sibling, it is now valid
          true ->
            io:format("sibling not updated, Pid is the same, [Old: ~s, New: ~s]~n", [Sibling#sibling.pid, Pid]),
            Sibling
       end;
     true ->
       Sibling
  end.


update_siblings(Id, Pid, Siblings) ->
  io:format("In update_siblings~n"),
  lists:foldl( fun(SibInList, Acc) -> [update_sibling(SibInList, Id, Pid)|Acc] end, [], Siblings).

update_sibling_invalidate(Sibling, Id) ->
  if Sibling#sibling.id == Id ->
       #sibling{id=Id,
                pid=Sibling#sibling.pid,
                leader=Sibling#sibling.leader,
                valid=false};
     true ->
       Sibling
  end.

get_sibling(Id, Siblings) ->
  hd(lists:filter( fun(SibInList ) -> SibInList#sibling.id == Id end, Siblings)).

update_siblings_invalidate(Id, Siblings) ->
  io:format("About to invalidate sibling ~s~n", [Id]),
  NewSiblings = lists:foldl( fun(SibInList, Acc) -> [update_sibling_invalidate(SibInList, Id)|Acc] end, [], Siblings),
%  NewSibling = get_sibling(Id, NewSiblings),
%  io:format("Sibling updated: [~s,~s]~n", NewSibling#sibling.id, NewSibling#sibling.valid),
  NewSiblings.

get_leader(Siblings) ->
  hd(lists:filter( fun(SibInList) -> SibInList#sibling.leader end, Siblings)).

% try to kill sibling, regardless of whether it is a leader or not
valid_to_kill(Siblings) ->
  lists:filter( fun(Sibling) -> Sibling#sibling.valid end, Siblings).
%  lists:filter( fun(Sibling) -> not Sibling#sibling.leader and Sibling#sibling.valid end, Siblings).

extract_pid(Sibling) ->
  Sibling#sibling.pid.

choose_random_sibling(Siblings) ->
  Index = random:uniform(length(Siblings)),
  lists:nth(Index, Siblings).

kill_pid(Pid) ->
  io:format("About to kill ~s ~n", [Pid]),
  os:cmd("sudo kill " ++ Pid).

start() ->
  start_server(9999).

start_server(Port) ->
  {ok, Listen} = gen_tcp:listen(Port, [binary, {active, false}, {reuseaddr, true}]),
  {ok, RiakPid} = riakc_pb_socket:start_link("10.100.2.1", 8087),
  Siblings = [],
  acceptor(Listen, Siblings, RiakPid),
  {ok}.

acceptor(ListenSocket, Siblings, RiakPid) ->
  case gen_tcp:accept(ListenSocket) of
  {ok, Socket} -> 
    {ok, NewSiblings} = handle(Socket, Siblings, RiakPid),
    acceptor(ListenSocket, NewSiblings, RiakPid)
  end.

handle(Socket, Siblings, RiakPid) ->
  inet:setopts(Socket, [{active, once}]),
  receive
    {tcp, Socket, <<"sibling ", Msg/binary>>} ->
      MsgStripped = string:strip(binary_to_list(Msg), both, $\n),
      MsgParsed = string:tokens(MsgStripped, " "),
      Sid = hd(MsgParsed),
      Spid = hd(tl(MsgParsed)),
      Sleader = hd(tl(tl(MsgParsed))),
      io:format("~s ~n", [Sid]),
      io:format("~s ~n", [Spid]),
      io:format("~s ~n", [Sleader]),
      NewSibling = new_sibling(Sid, Spid, Sleader),
      gen_tcp:close(Socket),
      {ok, [NewSibling|Siblings]};
    {tcp, Socket, <<"restart ", Msg/binary>>} ->
      MsgStripped = string:strip(binary_to_list(Msg), both, $\n),
      MsgParsed = string:tokens(MsgStripped, " "),
      Sid = hd(MsgParsed),
      Spid = hd(tl(MsgParsed)),
      io:format("~s ~n", [Sid]),
      io:format("~s ~n", [Spid]),
      NewSiblings = update_siblings(Sid, Spid, Siblings),
      gen_tcp:close(Socket),
      {ok, NewSiblings};
    {tcp, Socket, <<"kill", _/binary>>} ->
      if length(Siblings) =:= 3 ->
            SiblingsToKill = valid_to_kill(Siblings),
            if length(SiblingsToKill) =/= 0 ->
                  ChosenSibling = choose_random_sibling(SiblingsToKill),
                  NewSiblings = update_siblings_invalidate(ChosenSibling#sibling.id, Siblings),
                  gen_tcp:send(Socket, "1"),  % able to kill
                  kill_pid(extract_pid(ChosenSibling));
               true ->
                  io:format("No killable candidates~n"),
                  gen_tcp:send(Socket, "0"), % not able to kill
                  NewSiblings = Siblings
            end;
         true ->
            NewSiblings = Siblings,
            io:format("Not enough siblings ~n")
      end,
      gen_tcp:close(Socket),
      {ok, NewSiblings};
    {tcp, Socket, <<"reset", _/binary>>} ->
      io:format("Called reset~n"),
      gen_tcp:close(Socket),
      {ok, []};
    {tcp, Socket, <<"egress_id", _/binary>>} ->
      {ok, Obj} = riakc_pb_socket:get(RiakPid, {<<"strongly_consistent2">>, <<"ids">>}, <<"egress_id">>),
      Val = riakc_obj:get_value(Obj),
      io:format("egress xid: ~s ~n", [Val]),
      gen_tcp:send(Socket, Val),
      gen_tcp:close(Socket),
      {ok, Siblings};
    {tcp, Socket, Msg} ->
      io:format("~s ~n", [Msg]),
      gen_tcp:close(Socket),
      {ok, Siblings}
  end.
 
