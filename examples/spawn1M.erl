-module(spawn1M).
-export([start/0, heart_beat/0]).

heart_beat()->
    self() ! ok,
    receive
        ok -> ok
    end.

spawnHeart(0) -> ok;
spawnHeart(N) ->
    spawn(?MODULE, heart_beat, []),
    spawnHeart(N - 1).

start()->
    spawnHeart(1000000).
