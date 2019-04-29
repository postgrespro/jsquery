CREATE EXTENSION jsquery;

set escape_string_warning=off;
set standard_conforming_strings=on;

CREATE TABLE test_jsquery (v jsonb);

\copy test_jsquery from 'data/test_jsquery.data'

select 'asd.zzz = 13'::jsquery;
select 'asd.zzz < 13'::jsquery;
select 'asd(zzz < 13)'::jsquery;
select 'asd(zzz < 13 AND x = true)'::jsquery;
select 'asd(zzz < 13 AND x = "true")'::jsquery;
select 'asd(zzz < 13 AND x.zxc = "true")'::jsquery;
select 'asd(zzz < 13 OR #.zxc = "true" /* test */)'::jsquery;
select 'asd(* < 13 AND /* ttt */ #.zxc = "true")'::jsquery;
select '(* < 13 AND #.zxc = "true")'::jsquery;
select '* < 13 AND #.zxc/* t2 */ = "true"'::jsquery;
select '* < 13 AND #.zxc"x" = "true"'::jsquery;

select 'a < 1'::jsquery;
select 'a < -1'::jsquery;
select 'a < +1'::jsquery;
select 'a < .1'::jsquery;
select 'a < -.1'::jsquery;
select 'a < +.1'::jsquery;
select 'a < 0.1'::jsquery;
select 'a < -0.1'::jsquery;
select 'a < +0.1'::jsquery;
select 'a < 10.1'::jsquery;
select 'a < -10.1'::jsquery;
select 'a < +10.1'::jsquery;
select 'a < 1e1'::jsquery;
select 'a < -1e1'::jsquery;
select 'a < +1e1'::jsquery;
select 'a < .1e1'::jsquery;
select 'a < -.1e1'::jsquery;
select 'a < +.1e1'::jsquery;
select 'a < 0.1e1'::jsquery;
select 'a < -0.1e1'::jsquery;
select 'a < +0.1e1'::jsquery;
select 'a < 10.1e1'::jsquery;
select 'a < -10.1e1'::jsquery;
select 'a < +10.1e1'::jsquery;
select 'a < 1e-1'::jsquery;
select 'a < -1e-1'::jsquery;
select 'a < +1e-1'::jsquery;
select 'a < .1e-1'::jsquery;
select 'a < -.1e-1'::jsquery;
select 'a < +.1e-1'::jsquery;
select 'a < 0.1e-1'::jsquery;
select 'a < -0.1e-1'::jsquery;
select 'a < +0.1e-1'::jsquery;
select 'a < 10.1e-1'::jsquery;
select 'a < -10.1e-1'::jsquery;
select 'a < +10.1e-1'::jsquery;
select 'a < 1e+1'::jsquery;
select 'a < -1e+1'::jsquery;
select 'a < +1e+1'::jsquery;
select 'a < .1e+1'::jsquery;
select 'a < -.1e+1'::jsquery;
select 'a < +.1e+1'::jsquery;
select 'a < 0.1e+1'::jsquery;
select 'a < -0.1e+1'::jsquery;
select 'a < +0.1e+1'::jsquery;
select 'a < 10.1e+1'::jsquery;
select 'a < -10.1e+1'::jsquery;
select 'a < +10.1e+1'::jsquery;

select 'a in (0,1,2)'::jsquery;
select 'a IN (0,null, "null", xxx, "zzz", 2)'::jsquery;

select  'not < 1'::jsquery;
select  'not not < 1'::jsquery;
select  'not( not < 1)'::jsquery;
select  'not.x < 1'::jsquery;
select  'x.not < 1'::jsquery;
select  'a.%(not x > 0 and not (y < 0 or z = 0))'::jsquery;

select  'is < 1'::jsquery;
select  'in < 1'::jsquery;

select  'not is < 1'::jsquery;
select  'not in < 1'::jsquery;

select 'in in (1,2)'::jsquery;
select 'is in (1,2)'::jsquery;
select 'not in (1,2)'::jsquery;

select 'in is numeric'::jsquery;
select 'is is numeric'::jsquery;
select 'not is numeric'::jsquery;

select  'not.in  < 1'::jsquery;
select  'not.is  < 1'::jsquery;
select  'not.not < 1'::jsquery;
select  'in.in   < 1'::jsquery;
select  'in.is   < 1'::jsquery;
select  'in.not  < 1'::jsquery;
select  'is.in   < 1'::jsquery;
select  'is.is   < 1'::jsquery;
select  'is.not  < 1'::jsquery;

select 'a.b.#4 > 4'::jsquery;
select 'a.b.#10203.* > 4'::jsquery;

select '{"a": {"b": null}}'::jsonb @@ 'a.b = 1'::jsquery;
select '{"a": {"b": null}}'::jsonb @@ 'a.b = null'::jsquery;
select '{"a": {"b": null}}'::jsonb @@ 'a.b = false'::jsquery;
select '{"a": {"b": false}}'::jsonb @@ 'a.b = false'::jsquery;
select '{"a": {"b": false}}'::jsonb @@ 'a.b = true'::jsquery;
select '{"a": {"b": true}}'::jsonb @@ 'a.b = true'::jsquery;


select '{"a": {"b": 1}}'::jsonb @@ 'a.b = 1'::jsquery;
select '{"a": {"b": 1}}'::jsonb @@ 'a.b < 1'::jsquery;
select '{"a": {"b": 1}}'::jsonb @@ 'a.b <= 1'::jsquery;
select '{"a": {"b": 1}}'::jsonb @@ 'a.b >= 1'::jsquery;
select '{"a": {"b": 1}}'::jsonb @@ 'a.b > 1'::jsquery;

select '{"a": {"b": 1}}'::jsonb @@ 'a.b = 2'::jsquery;
select '{"a": {"b": 1}}'::jsonb @@ 'a.b < 2'::jsquery;
select '{"a": {"b": 1}}'::jsonb @@ 'a.b <= 2'::jsquery;
select '{"a": {"b": 1}}'::jsonb @@ 'a.b >= 2'::jsquery;
select '{"a": {"b": 1}}'::jsonb @@ 'a.b > 2'::jsquery;

select '{"a": {"b": 1}}'::jsonb @@ 'a.b = 0'::jsquery;
select '{"a": {"b": 1}}'::jsonb @@ 'a.b < 0'::jsquery;
select '{"a": {"b": 1}}'::jsonb @@ 'a.b <= 0'::jsquery;
select '{"a": {"b": 1}}'::jsonb @@ 'a.b >= 0'::jsquery;
select '{"a": {"b": 1}}'::jsonb @@ 'a.b > 0'::jsquery;

select '{"a": {"b": 1}}'::jsonb @@ '*.b > 0'::jsquery;
select '{"a": {"b": 1}}'::jsonb @@ '*.b > 0'::jsquery;
select '{"a": {"b": 1}}'::jsonb @@ 'a.* > 0'::jsquery;
select '{"a": {"b": 1}}'::jsonb @@ 'a.* > 0'::jsquery;

select '{"a": {"b": [1,2,3]}}'::jsonb @@ '*.b && [ 1 ]'::jsquery;
select '{"a": {"b": [1,2,3]}}'::jsonb @@ '*.b @> [ 1 ]'::jsquery;
select '{"a": {"b": [1,2,3]}}'::jsonb @@ '*.b <@ [ 1 ]'::jsquery;
select '{"a": {"b": [1,2,3]}}'::jsonb @@ '*.b @> [ 1,2,3,4 ]'::jsquery;
select '{"a": {"b": [1,2,3]}}'::jsonb @@ '*.b <@ [ 1,2,3,4 ]'::jsquery;

select '[{"a": 2}, {"a": 3}]'::jsonb @@ '*.a = 4'::jsquery;
select '[{"a": 2}, {"a": 3}]'::jsonb @@ '*.a = 3'::jsquery;

select '[{"a": 2}, {"a": 3}, {"a": {"a":4}}]'::jsonb @@ '#.a = 4'::jsquery;
select '[{"a": 2}, {"a": 3}, {"a": {"a":4}}]'::jsonb @@ '*.a = 4'::jsquery;

select '[{"a": 2}, {"a": 3}, {"a": {"a":4}}]'::jsonb @@ '#(a = 1 OR a=3)'::jsquery;
select '[{"a": 2}, {"a": 3}, {"a": {"a":4}}]'::jsonb @@ '#(a = 3 OR a=1)'::jsquery;
select '[{"a": 2}, {"a": 3}, {"a": {"a":4}}]'::jsonb @@ '#(a = 3 and a=1)'::jsquery;
select '[{"a": 2}, {"a": 3}, {"a": {"a":4}}]'::jsonb @@ '#(a = 3 and a=2)'::jsquery as "false";
select '[{"a": 2, "b":3}, {"a": 3, "b": 1}]'::jsonb @@ '#(b = 1 and a=3)'::jsquery;

select '[{"a": 2}, {"a": 3}, {"a": {"a":4}}]'::jsonb @@ '#.a.a = 4'::jsquery;
select '[{"a": 2}, {"a": 3}, {"a": {"a":4}}]'::jsonb @@ '*.a.a = 4'::jsquery;
select '[{"a": 2}, {"a": 3}, {"a": {"a":4}}]'::jsonb @@ '*.#.a.a = 4'::jsquery;
select '[{"a": 2}, {"a": 3}, {"a": {"a":4}}]'::jsonb @@ '#.*.a.a = 4'::jsquery;

select '{"a": 1}'::jsonb @@ 'a in (0,1,2)'::jsquery;
select '{"a": 1}'::jsonb @@ 'a in (0,2)'::jsquery;

select '{"a": {"b": [1,2,3]}}'::jsonb @@ 'a.b.#=2'::jsquery;
select '{"a": {"b": [1,2,3]}}'::jsonb @@ '*.b && [ 5 ]'::jsquery;

select '{"a": {"b": [1,2,3]}}'::jsonb @@ 'a=*'::jsquery;
select '{"a": {"b": [1,2,3]}}'::jsonb @@ 'a.b=*'::jsquery;
select '{"a": {"b": [1,2,3]}}'::jsonb @@ 'a.c=*'::jsquery;

select '{"a": {"b": [1,2,3]}}'::jsonb @@ 'a.b = [1,2,3]'::jsquery;
select '{"a": {"b": [1,2,3]}}'::jsonb @@ 'a.b.# = [1,2,3]'::jsquery;
select '{"a": {"b": [1,2,3]}}'::jsonb @@ 'a.b && [1,2,3]'::jsquery;
select '{"a": {"b": [1,2,3]}}'::jsonb @@ 'a.b.# && [1,2,3]'::jsquery;

select 'asd.# = 3'::jsquery & 'zzz = true' | 'xxx.# = zero';
select !'asd.# = 3'::jsquery & 'zzz = true' | !'xxx.# = zero';
select !'asd.#3.f = 3'::jsquery & 'zzz = true' | !'xxx.# = zero';

select '{"x":[0,1,1,2]}'::jsonb @@ 'x @> [1,0]'::jsquery;
select '{"x":[0,1,1,2]}'::jsonb @@ 'x @> [1,0,1]'::jsquery;
select '{"x":[0,1,1,2]}'::jsonb @@ 'x @> [1,0,3]'::jsquery;

select '{"a": {"b": [1,2,3]}}'::jsonb @@ '*.b && [ 2 ]'::jsquery;
select '{"a": {"b": [1,2,3]}}'::jsonb @@ '*.b($ && [ 2 ])'::jsquery;
select '{"a": {"b": [1,2,3]}}'::jsonb @@ 'a.$.b && [ 2 ]'::jsquery;
select '{"a": {"b": [1,2,3]}}'::jsonb @@ 'a.$.b ($ && [ 2 ])'::jsquery;

select '[1,2,3]'::jsonb @@ '# && [2]'::jsquery;
select '[1,2,3]'::jsonb @@ '#($ && [2])'::jsquery;
select '[1,2,3]'::jsonb @@ '$ && [2]'::jsquery;
select '[1,2,3]'::jsonb @@ '$ ($ && [2])'::jsquery;
select '[1,2,3]'::jsonb @@ '$ = 2'::jsquery;
select '[1,2,3]'::jsonb @@ '# = 2'::jsquery;
select '[1,2,3]'::jsonb @@ '#.$ = 2'::jsquery;
select '[1,2,3]'::jsonb @@ '#($ = 2)'::jsquery;

select '[3,4]'::jsonb @@ '#($ > 2 and $ < 5)'::jsquery;
select '[3,4]'::jsonb @@ '# > 2 and # < 5'::jsquery;
select '[1,6]'::jsonb @@ '#($ > 2 and $ < 5)'::jsquery;
select '[1,6]'::jsonb @@ '# > 2 and # < 5'::jsquery;

select '{"a": {"b": 3, "c": "hey"}, "x": [5,6]}'::jsonb @@ '%.b=3'::jsquery;
select '{"a": {"b": 3, "c": "hey"}, "x": [5,6]}'::jsonb @@ 'a.%=3'::jsquery;
select '{"a": {"b": 3, "c": "hey"}, "x": [5,6]}'::jsonb @@ '%.%="hey"'::jsquery;
select '{"a": {"b": 3, "c": "hey"}, "x": [5,6]}'::jsonb @@ '%="hey"'::jsquery;
select '{"a": {"b": 3, "c": "hey"}, "x": [5,6]}'::jsonb @@ '%=[5,6]'::jsquery;

select '{"a": {"b": [1,2,3]}}'::jsonb @@ 'a.b.#1 = 2'::jsquery;
select '{"a": {"b": [1,2,3]}}'::jsonb @@ 'a.b.#2 = 2'::jsquery;
select '{"a": {"b": [1,2,3]}}'::jsonb @@ 'a.b.#3 = 2'::jsquery;
select '{"a": {"b": [{"x":1},{"x":2},{"x":3}]}}'::jsonb @@ 'a.b.#1.x = 2'::jsquery;
select '{"a": {"b": [{"x":1},{"x":2},{"x":3}]}}'::jsonb @@ 'a.b.#2.x = 2'::jsquery;
select '{"a": {"b": [{"x":1},{"x":2},{"x":3}]}}'::jsonb @@ 'a.b.#3.x = 2'::jsquery;

select '"XXX"'::jsonb @@ '$="XXX"'::jsquery;
select '"XXX"'::jsonb @@ '#.$="XXX"'::jsquery;

--Unicode

select 'a\t = "dollar \u0024 character"'::jsquery;
select '{ "a":  "dollar \u0024 character" }'::jsonb @@ '* = "dollar \u0024 character"'::jsquery;
select '{ "a":  "dollar \u0024 character" }'::jsonb @@ '* = "dollar $ character"'::jsquery;
select '{ "a":  "dollar $ character" }'::jsonb @@ '* = "dollar \u0024 character"'::jsquery;
select 'a\r = "\n\""'::jsquery;
select 'a\r = "\u0000"'::jsquery;
select 'a\r = \u0000'::jsquery;
select 'a\r = "\abcd"'::jsquery AS err;
select 'a\r = "\\abcd"'::jsquery;
select 'a\r = "x\u0000"'::jsquery;
select 'a\r = x\u0000'::jsquery;
select 'a\r = "x\abcd"'::jsquery AS err;
select 'a\r = "x\\abcd"'::jsquery;
select 'a\r = "x\u0000x"'::jsquery;
select 'a\r = x\u0000x'::jsquery;
select 'a\r = "x\abcdx"'::jsquery AS err;
select 'a\r = "x\\abcdx"'::jsquery;
select 'a\r = "\u0000x"'::jsquery;
select 'a\r = \u0000x'::jsquery;
select 'a\r = "\abcdx"'::jsquery AS err;
select 'a\r = "\\abcdx"'::jsquery;
select 'a\r = x"\\abcd"'::jsquery AS err;

--IS

select  'as IS boolean OR as is ARRAY OR as is ObJect OR as is Numeric OR as is string'::jsquery;
select '{"as": "xxx"}' @@ 'as IS string'::jsquery;
select '{"as": "xxx"}' @@ 'as IS boolean OR as is ARRAY OR as is ObJect OR as is Numeric'::jsquery;
select '{"as": 5}' @@ 'as is Numeric'::jsquery;
select '{"as": true}' @@ 'as is boolean'::jsquery;
select '{"as": false}' @@ 'as is boolean'::jsquery;
select '{"as": "false"}' @@ 'as is boolean'::jsquery;
select '["xxx"]' @@ '$ IS array'::jsquery;
select '{"as": false}' @@ '$ IS object'::jsquery;
select '"xxx"' @@ '$ IS string'::jsquery;
select '"xxx"' @@ '$ IS numeric'::jsquery;

--hint

select 'a /*-- noindex */ = 5'::jsquery;
select 'a /*-- index */ = 5'::jsquery;
select 'asd.# = 3'::jsquery & 'zzz /*-- noindex */ = true' | 'xxx.# /*-- index */ = zero';
select 'a /*-- xxx */ = 5'::jsquery;
select 'a /* index */ = 5'::jsquery;
select 'a /* noindex */ = 5'::jsquery;
select 'a = /*-- noindex */ 5'::jsquery;
select 'a = /* noindex */ 5'::jsquery;

--LENGTH
select 'a.@# = 4'::jsquery;
select 'a.@#.$ = 4'::jsquery as noerror;
select 'a.@#.a = 4'::jsquery as error;
select 'a.@#.* = 4'::jsquery as error;
select 'a.@#.% = 4'::jsquery as error;
select 'a.@#.# = 4'::jsquery as error;
select 'a.@#.*: = 4'::jsquery as error;
select 'a.@#.%: = 4'::jsquery as error;
select 'a.@#.#: = 4'::jsquery as error;
select 'a.@# (a = 5 or b = 6)'::jsquery as error;
select '[]' @@ '@# = 0'::jsquery;
select '[]' @@ '@# < 2'::jsquery;
select '[]' @@ '@# > 1'::jsquery;
select '[1]' @@ '@# = 0'::jsquery;
select '[1]' @@ '@# < 2'::jsquery;
select '[1]' @@ '@# > 1'::jsquery;
select '[1,2]' @@ '@# = 0'::jsquery;
select '[1,2]' @@ '@# < 2'::jsquery;
select '[1,2]' @@ '@# > 1'::jsquery;
select '[1,2]' @@ '@# in (1, 2)'::jsquery;
select '[1,2]' @@ '@# in (1, 3)'::jsquery;
select '{"a":[1,2]}' @@ '@# in (2, 4)'::jsquery;
select '{"a":[1,2]}' @@ 'a.@# in (2, 4)'::jsquery;
select '{"a":[1,2]}' @@ '%.@# in (2, 4)'::jsquery;
select '{"a":[1,2]}' @@ '*.@# in (2, 4)'::jsquery;
select '{"a":[1,2]}' @@ '*.@# ($ = 4 or $ = 2)'::jsquery;
select '{"a":[1,2]}' @@ '@#  = 1'::jsquery;

--filter
select '?( not b>0). x'::jsquery;
select 'a.?(b>0 and x= 0 ) .c'::jsquery;
select 'a.$. ?(b>0 and x= 0 ) . c.k'::jsquery;
select 'a.$.? (b>0 and x.*= 0 ).c.k'::jsquery;
select '[{"a":1, "b":10}, {"a":2, "b":20}, {"a":3, "b":30}]'::jsonb @@ '#. ?(a < 0) (b=20)'::jsquery;
select '[{"a":1, "b":10}, {"a":2, "b":20}, {"a":3, "b":30}]'::jsonb @@ '#. ?(a > 0) (b=20)'::jsquery;
select '[{"a":1, "b":10}, {"a":2, "b":20}, {"a":3, "b":30}]'::jsonb @@ '#. ?(a > 1) (b=20)'::jsquery;
select '[{"a":1, "b":10}, {"a":2, "b":20}, {"a":3, "b":30}]'::jsonb @@ '#. ?(a > 2) (b=20)'::jsquery;
select '[{"a":1, "b":10}, {"a":2, "b":20}, {"a":3, "b":30}]'::jsonb @@ '#. ?(a > 3) (b=20)'::jsquery;
select '[{"a":1, "b":10}, {"a":2, "b":20}]'::jsonb ~~ '#.a';
select '[{"a":1, "b":10}, {"a":2, "b":20}]'::jsonb ~~ '#. ?(a > 1). b';
select '[{"a":1, "b":10}, {"a":2, "b":20}, {"a":3, "b":30}]'::jsonb ~~ '# . ?(a > 1)';
select '[{"a":1, "b":10}, {"a":2, "b":20}, {"a":3, "b":30}]'::jsonb ~~ '%';
select '{"a":1, "b":2, "c":3}'::jsonb ~~ '%';
select '{"a":1, "b":2, "c":3}'::jsonb ~~ '% . ? ( $ > 2 )';
select '{"a":1, "b":2, "c":3}'::jsonb ~~ '% . ? ( $ > 2 ).$';
select '{"a":1, "b":2, "c":3}'::jsonb ~~ '? ( % > 2 )';
select '{"a":1, "b":2, "c":3}'::jsonb ~~ '? ( %: > 0 )';
select '{"a":1, "b":2, "c":3}'::jsonb ~~ '? ( %: > 2 )';
select '[{"a":1, "b":10}, {"a":2, "b":20}, {"a":3, "b":30}]'::jsonb ~~ '#';
select '[1,2,3]'::jsonb ~~ '#';
select '[1,2,3]'::jsonb ~~ '#. ?($ > 2)';
select '[1,2,3]'::jsonb ~~ '#. ?($ > 2).$';
select '[1,2,3]'::jsonb ~~ ' ?(#.$ > 2).$';
select '[1,2,3]'::jsonb ~~ ' ?(#:.$ > 2).$';
select '[1,2,3]'::jsonb ~~ ' ?(#:.$ > 0).$';
select '{"a": {"b": {"c": 1}}}'::jsonb ~~ '*.?(c >0)';
select '{"a": {"b": {"c": 1}}}'::jsonb ~~ '?(*.c >0)';
select '{"tags":[{"term":["NYC", "CYN"]}, {"term":["1NYC", "1CYN"]} ]}'::jsonb ~~ 'tags.#.term.#. ? ( $ = "NYC")';
select '{"tags":[{"term":["NYC", "CYN"]}, {"term":["1NYC", "1CYN"]} ]}'::jsonb ~~ 'tags.#.term. ? ( # = "NYC")';

--ALL
select 'a.*: = 4'::jsquery;
select '%: = 4'::jsquery;
select '#:.i = 4'::jsquery;
select '[]' @@ '#: ($ > 1 and $ < 5)'::jsquery;
select '[2,3,4]' @@ '#: ($ > 1 and $ < 5)'::jsquery;
select '[2,3,5]' @@ '#: ($ > 1 and $ < 5)'::jsquery;
select '[2,3,5]' @@ '# ($ > 1 and $ < 5)'::jsquery;
select '[2,3,"x"]' @@ '#: ($ > 1 and $ < 5)'::jsquery;
select '{}' @@ '%: ($ > 1 and $ < 5)'::jsquery;
select '{}' @@ '*: ($ is object)'::jsquery;
select '"a"' @@ '*: is string'::jsquery;
select '1' @@ '*: is string'::jsquery;
select '{"a":2,"b":3,"c":4}' @@ '%: ($ > 1 and $ < 5)'::jsquery;
select '{"a":2,"b":3,"c":5}' @@ '%: ($ > 1 and $ < 5)'::jsquery;
select '{"a":2,"b":3,"c":5}' @@ '% ($ > 1 and $ < 5)'::jsquery;
select '{"a":2,"b":3,"c":"x"}' @@ '%: ($ > 1 and $ < 5)'::jsquery;
select '{"a":2,"b":3,"c":4}' @@ '*: ($ > 1 and $ < 5)'::jsquery;
select '{"a":2,"b":3,"c":5}' @@ '*: ($ > 1 and $ < 5)'::jsquery;
select '{"a":2,"b":3,"c":4}' @@ '*: ($ is object OR ($> 1 and $ < 5))'::jsquery;
select '{"a":2,"b":3,"c":5}' @@ '*: ($ is object OR ($> 1 and $ < 5))'::jsquery;
select '{"b":{"ba":3, "bb":4}}' @@ '*: ($ is object OR ($ > 1 and $ < 5))'::jsquery;
select '{"b":{"ba":3, "bb":5}}' @@ '*: ($ is object OR ($> 1 and $ < 5))'::jsquery;
select '{"a":{"aa":1, "ab":2}, "b":{"ba":3, "bb":4}}' @@ '*: ($ is object OR ($ > 0 and $ < 5))'::jsquery;
select '{"a":{"aa":1, "ab":2}, "b":{"ba":3, "bb":5}}' @@ '*: ($ is object OR ($> 0 and $ < 5))'::jsquery;
select '{"a":{"aa":1, "ab":2}, "b":{"ba":3, "bb":5}}' @@ '* ($ > 0 and $ < 5)'::jsquery;
select '{"a":{"aa":1, "ab":2}, "b":{"ba":3, "bb":5}}' @@ '*: ($ is object OR $ is numeric)'::jsquery;
select '{"a":{"aa":1, "ab":2}, "b":[5,6]}' @@ '*: ($ is object OR $ is numeric)'::jsquery;
select '{"a":{"aa":1, "ab":2}, "b":[5,6]}' @@ '*: ($ is object OR $ is array OR $ is numeric)'::jsquery;
select '{"a":{"aa":1, "ab":2}, "b":[5,6, {"c":8}]}' @@ '*: ($ is object OR $ is array OR $ is numeric)'::jsquery;
select '{"a":{"aa":1, "ab":2}, "b":[5,6, {"c":"x"}]}' @@ '*: ($ is object OR $ is array OR $ is numeric)'::jsquery;
select '{"a":{"aa":1, "ab":2}, "b":[5,6, {"c":null}]}' @@ '*: ($ is object OR $ is array OR $ is numeric)'::jsquery;
select '{"a":{"aa":1}, "b":{"aa":1, "bb":2}}' @@ '%:.aa is numeric'::jsquery;
select '{"a":{"aa":1}, "b":{"aa":true, "bb":2}}' @@ '%:.aa is numeric'::jsquery;
select '{"a":{"aa":1}, "b":{"aa":1, "bb":2}, "aa":16}' @@ '*: (not $ is object or $.aa is numeric)'::jsquery;
select '{"a":{"aa":1}, "b":{"aa":1, "bb":2}}' @@ '*: (not $ is object or $.aa is numeric or % is object)'::jsquery;
SELECT 'test.# IN (1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64)'::jsquery;

select '[]' @@ '(@# > 0 and #: = 16)'::jsquery;
select '[16]' @@ '(@# > 0 and #: = 16)'::jsquery;

select '{"a": {"b": 1, "c": 2}, "b": {"d":3}}'::jsonb @@ 'a.b or b.d'::jsquery;
select '{"a": {"b": 1, "c": 2}, "b": {"d":3}}'::jsonb @@ 'a.c or b.d'::jsquery;
select '{"a": {"b": 1, "c": 2}, "b": {"d":3}}'::jsonb @@ 'a.g or b.d'::jsquery;
select '{"a": {"b": 1, "c": 2}, "b": {"d":3}}'::jsonb @@ 'a.g or b.c'::jsquery;
select '{"a": {"b": 1, "c": 2}, "b": {"d":3}}'::jsonb @@ 'a.b and b.d'::jsquery;
select '{"a": {"b": 1, "c": 2}, "b": {"d":3}}'::jsonb @@ 'a.c and b.d'::jsquery;
select '{"a": {"b": 1, "c": 2}, "b": {"d":3}}'::jsonb @@ 'a.c and b.b'::jsquery;
select '{"a": {"b": 1, "c": 2}, "b": {"d":3}}'::jsonb @@ 'a.g and b.d'::jsquery;

select '{"a": {"b": 1, "c": 2}, "b": {"d":3}}'::jsonb ~~ 'a.b and b.d';
select '{"a": {"b": 1, "c": 2}, "b": {"d":3}}'::jsonb ~~ 'a.b and b.c';
select '{"a": {"b": 1, "c": 2}, "b": {"d":3}}'::jsonb ~~ 'a.b and (b.d or b.c)';
select '{"a": {"b": 1, "c": 2}, "b": {"d":3}}'::jsonb ~~ 'a.b and (b.c or b.d)';
select '{"a": {"b": 1, "c": 2}, "b": {"d":3}}'::jsonb ~~ 'a.b and a.c and b.d';
select '{"a": {"b": 1, "c": 2}, "b": {"d":3}}'::jsonb ~~ '(a.b or a.c) and b.d';
select '{"a": {"b": 1, "c": 2}, "b": {"d":3}}'::jsonb ~~ '(a.e or a.c) and b.d';
select '{"a": {"b": 1, "c": 2}, "b": {"d":3}}'::jsonb ~~ '(a.e or a.g) and b.d';
select '{"a": {"b": 1, "c": 2}, "b": {"d":3}}'::jsonb ~~ 'a.b or (b.d or b.c)';
select '{"a": {"b": 1, "c": 2}, "b": {"d":3}}'::jsonb ~~ 'b.d or (a.b or a.c)';
select '{"a": {"b": 1, "c": 2}, "b": {"d":3}}'::jsonb ~~ 'b.d or (a.b and a.c)';
select '{"a": {"b": 1, "c": 2}, "b": {"d":3}}'::jsonb ~~ 'b.f or (a.b and a.c)';
select '{"a": {"b": 1, "c": 2}, "b": {"d":3}}'::jsonb ~~ 'b.d and (a.b and a.c)';
select '{"a": {"b": [6,5,4], "c": 2}, "b": {"d":3}}'::jsonb ~~ 'b.d and (a.b and a.c)';

--extract entries for index scan

SELECT gin_debug_query_path_value('NOT NOT NOT x(y(NOT (a=1) and NOT (b=2)) OR NOT NOT (c=3)) and z = 5');
SELECT gin_debug_query_path_value('NOT #(x=1) and NOT *(y=1) and NOT %(z=1) ');
SELECT gin_debug_query_path_value('#(NOT x=1) and *(NOT y=1) and %(NOT z=1) ');
SELECT gin_debug_query_path_value('NOT #(NOT x=1) and NOT *(NOT y=1) and NOT %(NOT z=1) ');
SELECT gin_debug_query_path_value('#(x = "a" and y > 0 and y < 1 and z > 0)');
SELECT gin_debug_query_path_value('#(x = "a" and y /*-- index */ >= 0 and y < 1 and z > 0)');
SELECT gin_debug_query_path_value('#(x /*-- noindex */ = "a" and y > 0 and y <= 1 and z /*-- index */ > 0)');
SELECT gin_debug_query_path_value('x = 1 and (y /*-- index */ > 0 and y < 1 OR z > 0)');
SELECT gin_debug_query_path_value('%.x = 1');
SELECT gin_debug_query_path_value('*.x = "b"');
SELECT gin_debug_query_path_value('x && [1,2,3]');
SELECT gin_debug_query_path_value('x @> [1,2,3]');
SELECT gin_debug_query_path_value('x <@ [1,2,3]');
SELECT gin_debug_query_path_value('x = *');
SELECT gin_debug_query_path_value('x is boolean');
SELECT gin_debug_query_path_value('x is string');
SELECT gin_debug_query_path_value('x is numeric');
SELECT gin_debug_query_path_value('x is array');
SELECT gin_debug_query_path_value('x is object');
SELECT gin_debug_query_path_value('#:(x=1) AND %:(y=1) AND *:(z=1)');
SELECT gin_debug_query_path_value('#:(NOT x=1) AND %:(NOT y=1) AND *:(NOT z=1)');
SELECT gin_debug_query_path_value('NOT #:(NOT x=1) AND NOT %:(NOT y=1) AND NOT *:(NOT z=1)');
SELECT gin_debug_query_path_value('$ = true');
SELECT gin_debug_query_path_value('$ . ? (review_votes > 10) . review_rating < 7');
SELECT gin_debug_query_path_value('similar_product_ids . ? (# = "B0002W4TL2") . $');

SELECT gin_debug_query_laxpath_value('NOT NOT NOT x(y(NOT (a=1) and NOT (b=2)) OR NOT NOT (c=3)) and z = 5');
SELECT gin_debug_query_laxpath_value('NOT #(x=1) and NOT *(y=1) and NOT %(z=1) ');
SELECT gin_debug_query_laxpath_value('#(NOT x=1) and *(NOT y=1) and %(NOT z=1) ');
SELECT gin_debug_query_laxpath_value('NOT #(NOT x=1) and NOT *(NOT y=1) and NOT %(NOT z=1) ');
SELECT gin_debug_query_laxpath_value('#(x = "a" and y > 0 and y < 1 and z > 0)');
SELECT gin_debug_query_laxpath_value('#(x = "a" and y /*-- index */ >= 0 and y < 1 and z > 0)');
SELECT gin_debug_query_laxpath_value('#(x /*-- noindex */ = "a" and y > 0 and y <= 1 and z /*-- index */ > 0)');
SELECT gin_debug_query_laxpath_value('x = 1 and (y /*-- index */ > 0 and y < 1 OR z > 0)');
SELECT gin_debug_query_laxpath_value('%.x = 1');
SELECT gin_debug_query_laxpath_value('*.x = "b"');
SELECT gin_debug_query_laxpath_value('x && [1,2,3]');
SELECT gin_debug_query_laxpath_value('x @> [1,2,3]');
SELECT gin_debug_query_laxpath_value('x <@ [1,2,3]');
SELECT gin_debug_query_laxpath_value('x = *');
SELECT gin_debug_query_laxpath_value('x is boolean');
SELECT gin_debug_query_laxpath_value('x is string');
SELECT gin_debug_query_laxpath_value('x is numeric');
SELECT gin_debug_query_laxpath_value('x is array');
SELECT gin_debug_query_laxpath_value('x is object');
SELECT gin_debug_query_laxpath_value('#:(x=1) AND %:(y=1) AND *:(z=1)');
SELECT gin_debug_query_laxpath_value('#:(NOT x=1) AND %:(NOT y=1) AND *:(NOT z=1)');
SELECT gin_debug_query_laxpath_value('NOT #:(NOT x=1) AND NOT %:(NOT y=1) AND NOT *:(NOT z=1)');
SELECT gin_debug_query_laxpath_value('$ = true');
SELECT gin_debug_query_laxpath_value('$ . ? (review_votes > 10) . review_rating < 7');
SELECT gin_debug_query_laxpath_value('similar_product_ids . ? (# = "B0002W4TL2") . $');

SELECT gin_debug_query_value_path('NOT NOT NOT x(y(NOT (a=1) and NOT (b=2)) OR NOT NOT (c=3)) and z = 5');
SELECT gin_debug_query_value_path('NOT #(x=1) and NOT *(y=1) and NOT %(z=1) ');
SELECT gin_debug_query_value_path('#(NOT x=1) and *(NOT y=1) and %(NOT z=1) ');
SELECT gin_debug_query_value_path('NOT #(NOT x=1) and NOT *(NOT y=1) and NOT %(NOT z=1) ');
SELECT gin_debug_query_value_path('#(x = "a" and y > 0 and y < 1 and z > 0)');
SELECT gin_debug_query_value_path('#(x = "a" and y /*-- index */ >= 0 and y < 1 and z > 0)');
SELECT gin_debug_query_value_path('#(x /*-- noindex */ = "a" and y > 0 and y <= 1 and z /*-- index */ > 0)');
SELECT gin_debug_query_value_path('x = 1 and (y /*-- index */ > 0 and y < 1 OR z > 0)');
SELECT gin_debug_query_value_path('%.x = 1');
SELECT gin_debug_query_value_path('*.x = "b"');
SELECT gin_debug_query_value_path('x && [1,2,3]');
SELECT gin_debug_query_value_path('x @> [1,2,3]');
SELECT gin_debug_query_value_path('x <@ [1,2,3]');
SELECT gin_debug_query_value_path('x = [1,2,3]');
SELECT gin_debug_query_value_path('x = *');
SELECT gin_debug_query_value_path('x is boolean');
SELECT gin_debug_query_value_path('x is string');
SELECT gin_debug_query_value_path('x is numeric');
SELECT gin_debug_query_value_path('x is array');
SELECT gin_debug_query_value_path('x is object');
SELECT gin_debug_query_value_path('#:(x=1) AND %:(y=1) AND *:(z=1)');
SELECT gin_debug_query_value_path('#:(NOT x=1) AND %:(NOT y=1) AND *:(NOT z=1)');
SELECT gin_debug_query_value_path('NOT #:(NOT x=1) AND NOT %:(NOT y=1) AND NOT *:(NOT z=1)');
SELECT gin_debug_query_value_path('(@# > 0 and #: = 16)');
SELECT gin_debug_query_value_path('*.@# ($ = 4 or $ = 2)');
SELECT gin_debug_query_value_path('tags.#.term. ? ( # = "NYC").x > 0');
SELECT gin_debug_query_value_path('$ = true');
SELECT gin_debug_query_value_path('$ . ? (review_votes > 10) . review_rating < 7');
SELECT gin_debug_query_value_path('similar_product_ids . ? (# = "B0002W4TL2") . $');

-- -extract jsonpath entries for index scan

SELECT gin_debug_jsonpath_value_path('lax    $');
SELECT gin_debug_jsonpath_value_path('strict $');
SELECT gin_debug_jsonpath_value_path('lax    $.a');
SELECT gin_debug_jsonpath_value_path('strict $.a');
SELECT gin_debug_jsonpath_value_path('lax    exists($)');
SELECT gin_debug_jsonpath_value_path('strict exists($)');
SELECT gin_debug_jsonpath_value_path('lax    exists($.a)');
SELECT gin_debug_jsonpath_value_path('strict exists($.a)');
SELECT gin_debug_jsonpath_value_path('lax    exists($.a.b)');
SELECT gin_debug_jsonpath_value_path('strict exists($.a.b)');

SELECT gin_debug_jsonpath_value_path('lax    exists($ ? (@.b == 3).c.d)');
SELECT gin_debug_jsonpath_value_path('strict exists($ ? (@.b == 3).c.d)');
SELECT gin_debug_jsonpath_value_path('lax    exists($.a ? (@.b == 3).c.d ? (@.e == "aa" || @ == 1))');
SELECT gin_debug_jsonpath_value_path('strict exists($.a ? (@.b == 3).c.d ? (@.e == "aa" || @ == 1))');

SELECT gin_debug_jsonpath_value_path('lax    $.a ? (@.b == "foo").c == 1');
SELECT gin_debug_jsonpath_value_path('strict $.a ? (@.b == "foo").c == 1');
SELECT gin_debug_jsonpath_value_path('lax    $.a ? (@.b == "foo") == 1');
SELECT gin_debug_jsonpath_value_path('strict $.a ? (@.b == "foo") == 1');
SELECT gin_debug_jsonpath_value_path('lax    $.a ? (@.b == "foo")');
SELECT gin_debug_jsonpath_value_path('strict $.a ? (@.b == "foo")');
SELECT gin_debug_jsonpath_value_path('lax    exists($.a ? (@.b == "foo"))');
SELECT gin_debug_jsonpath_value_path('strict exists($.a ? (@.b == "foo"))');
SELECT gin_debug_jsonpath_value_path('lax    exists($.a ? (@.b == "foo").c)');
SELECT gin_debug_jsonpath_value_path('strict exists($.a ? (@.b == "foo").c)');
select gin_debug_jsonpath_value_path('lax    exists($.a.b ? (@.c.d == 1))');
select gin_debug_jsonpath_value_path('strict exists($.a.b ? (@.c.d == 1))');

select gin_debug_jsonpath_value_path('$.a > 1');

SELECT gin_debug_jsonpath_value_path('lax $.a == $.b');
SELECT gin_debug_jsonpath_value_path('strict $.a == $.b');

SELECT gin_debug_jsonpath_path_value('lax    $');
SELECT gin_debug_jsonpath_path_value('strict $');
SELECT gin_debug_jsonpath_path_value('lax    $.a');
SELECT gin_debug_jsonpath_path_value('strict $.a');
SELECT gin_debug_jsonpath_path_value('lax    exists($)');
SELECT gin_debug_jsonpath_path_value('strict exists($)');
SELECT gin_debug_jsonpath_path_value('lax    exists($.a)');
SELECT gin_debug_jsonpath_path_value('strict exists($.a)');
SELECT gin_debug_jsonpath_path_value('lax    exists($.a.b)');
SELECT gin_debug_jsonpath_path_value('strict exists($.a.b)');

SELECT gin_debug_jsonpath_path_value('lax    exists($ ? (@.b == 3).c.d)');
SELECT gin_debug_jsonpath_path_value('strict exists($ ? (@.b == 3).c.d)');
SELECT gin_debug_jsonpath_path_value('lax    exists($.a ? (@.b == 3).c.d ? (@.e == "aa" || @ == 1))');
SELECT gin_debug_jsonpath_path_value('strict exists($.a ? (@.b == 3).c.d ? (@.e == "aa" || @ == 1))');

SELECT gin_debug_jsonpath_path_value('lax    $.a ? (@.b == "foo").c == 1');
SELECT gin_debug_jsonpath_path_value('strict $.a ? (@.b == "foo").c == 1');
SELECT gin_debug_jsonpath_path_value('lax    $.a ? (@.b == "foo") == 1');
SELECT gin_debug_jsonpath_path_value('strict $.a ? (@.b == "foo") == 1');
SELECT gin_debug_jsonpath_path_value('lax    $.a ? (@.b == "foo")');
SELECT gin_debug_jsonpath_path_value('strict $.a ? (@.b == "foo")');
SELECT gin_debug_jsonpath_path_value('lax    exists($.a ? (@.b == "foo"))');
SELECT gin_debug_jsonpath_path_value('strict exists($.a ? (@.b == "foo"))');
SELECT gin_debug_jsonpath_path_value('lax    exists($.a ? (@.b == "foo").c)');
SELECT gin_debug_jsonpath_path_value('strict exists($.a ? (@.b == "foo").c)');
select gin_debug_jsonpath_path_value('strict exists($.a.b ? (@.c.d == 1))');
select gin_debug_jsonpath_path_value('lax    exists($.a.b ? (@.c.d == 1))');

select gin_debug_jsonpath_path_value('strict $.a.b == 1');
select gin_debug_jsonpath_path_value('lax    $.a.b == 1');
select gin_debug_jsonpath_path_value('strict $.a.b[*] == 1');
select gin_debug_jsonpath_path_value('lax    $.a.b[*] == 1');
select gin_debug_jsonpath_path_value('strict $.a[*].b[*] == 1');
select gin_debug_jsonpath_path_value('lax    $.a[*].b[*] == 1');
select gin_debug_jsonpath_path_value('strict $.a.b[*][*] == 1');
select gin_debug_jsonpath_path_value('lax    $.a.b[*][*] == 1');
select gin_debug_jsonpath_path_value('strict $.a[*].b[*] == 1');
select gin_debug_jsonpath_path_value('lax    $.a[*].b[*] == 1');

SELECT gin_debug_jsonpath_path_value('lax    $.a > 1 && $.a < 2 && $.b >= 3 && $.c <= "aaa"');
SELECT gin_debug_jsonpath_path_value('strict $.a > 1 && $.a < 2 && $.b >= 3 && $.c <= "aaa"');

SELECT gin_debug_jsonpath_path_value('lax $.a == $.b');
SELECT gin_debug_jsonpath_path_value('strict $.a == $.b');

SELECT gin_debug_jsonpath_laxpath_value('lax    $');
SELECT gin_debug_jsonpath_laxpath_value('strict $');
SELECT gin_debug_jsonpath_laxpath_value('lax    $.a');
SELECT gin_debug_jsonpath_laxpath_value('strict $.a');
SELECT gin_debug_jsonpath_laxpath_value('lax    exists($)');
SELECT gin_debug_jsonpath_laxpath_value('strict exists($)');
SELECT gin_debug_jsonpath_laxpath_value('lax    exists($.a)');
SELECT gin_debug_jsonpath_laxpath_value('strict exists($.a)');
SELECT gin_debug_jsonpath_laxpath_value('lax    exists($.a.b)');
SELECT gin_debug_jsonpath_laxpath_value('strict exists($.a.b)');

SELECT gin_debug_jsonpath_laxpath_value('lax    exists($ ? (@.b == 3).c.d)');
SELECT gin_debug_jsonpath_laxpath_value('strict exists($ ? (@.b == 3).c.d)');
SELECT gin_debug_jsonpath_laxpath_value('lax    exists($.a ? (@.b == 3).c.d ? (@.e == "aa" || @ == 1))');
SELECT gin_debug_jsonpath_laxpath_value('strict exists($.a ? (@.b == 3).c.d ? (@.e == "aa" || @ == 1))');

SELECT gin_debug_jsonpath_laxpath_value('lax    $.a ? (@.b == "foo").c == 1');
SELECT gin_debug_jsonpath_laxpath_value('strict $.a ? (@.b == "foo").c == 1');
SELECT gin_debug_jsonpath_laxpath_value('lax    $.a ? (@.b == "foo") == 1');
SELECT gin_debug_jsonpath_laxpath_value('strict $.a ? (@.b == "foo") == 1');
SELECT gin_debug_jsonpath_laxpath_value('lax    $.a ? (@.b == "foo")');
SELECT gin_debug_jsonpath_laxpath_value('strict $.a ? (@.b == "foo")');
SELECT gin_debug_jsonpath_laxpath_value('lax    exists($.a ? (@.b == "foo"))');
SELECT gin_debug_jsonpath_laxpath_value('strict exists($.a ? (@.b == "foo"))');
SELECT gin_debug_jsonpath_laxpath_value('lax    exists($.a ? (@.b == "foo").c)');
SELECT gin_debug_jsonpath_laxpath_value('strict exists($.a ? (@.b == "foo").c)');
select gin_debug_jsonpath_laxpath_value('exists($.a.b ? (@.c.d == 1))');

select gin_debug_jsonpath_laxpath_value('strict $.a.b == 1');
select gin_debug_jsonpath_laxpath_value('lax    $.a.b == 1');
select gin_debug_jsonpath_laxpath_value('strict $.a.b[*] == 1');
select gin_debug_jsonpath_laxpath_value('lax    $.a.b[*] == 1');
select gin_debug_jsonpath_laxpath_value('strict $.a[*].b[*] == 1');
select gin_debug_jsonpath_laxpath_value('lax    $.a[*].b[*] == 1');
select gin_debug_jsonpath_laxpath_value('strict $.a.b[*][*] == 1');
select gin_debug_jsonpath_laxpath_value('lax    $.a.b[*][*] == 1');
select gin_debug_jsonpath_laxpath_value('strict $.a[*].b[*] == 1');
select gin_debug_jsonpath_laxpath_value('lax    $.a[*].b[*] == 1');

SELECT gin_debug_jsonpath_laxpath_value('lax    $.a > 1 && $.a < 2 && $.b >= 3 && $.c <= "aaa"');
SELECT gin_debug_jsonpath_laxpath_value('strict $.a > 1 && $.a < 2 && $.b >= 3 && $.c <= "aaa"');

SELECT gin_debug_jsonpath_laxpath_value('lax $.a == $.b');
SELECT gin_debug_jsonpath_laxpath_value('strict $.a == $.b');

---table and index

select count(*) from test_jsquery where (v->>'review_helpful_votes')::int4 > 0;
select count(*) from test_jsquery where (v->>'review_helpful_votes')::int4 > 19;
select count(*) from test_jsquery where (v->>'review_helpful_votes')::int4 < 19;
select count(*) from test_jsquery where (v->>'review_helpful_votes')::int4 >= 19;
select count(*) from test_jsquery where (v->>'review_helpful_votes')::int4 <= 19;
select count(*) from test_jsquery where (v->>'review_helpful_votes')::int4 = 19;
select count(*) from test_jsquery where (v->>'review_helpful_votes')::int4 > 16 AND
										(v->>'review_helpful_votes')::int4 < 20;


select count(*) from test_jsquery where v @@ 'review_helpful_votes > 0'::jsquery;
select count(*) from test_jsquery where v @@ 'review_helpful_votes > 19'::jsquery;
select count(*) from test_jsquery where v @@ 'review_helpful_votes < 19'::jsquery;
select count(*) from test_jsquery where v @@ 'review_helpful_votes >= 19'::jsquery;
select count(*) from test_jsquery where v @@ 'review_helpful_votes <= 19'::jsquery;
select count(*) from test_jsquery where v @@ 'review_helpful_votes = 19'::jsquery;
select count(*) from test_jsquery where v @@ 'review_helpful_votes > 16'::jsquery AND
										v @@ 'review_helpful_votes < 20'::jsquery;
select count(*) from test_jsquery where v @@ 'review_helpful_votes > 16 and review_helpful_votes < 20'::jsquery;
select count(*) from test_jsquery where v @@ 'review_helpful_votes ($ > 16 and $ < 20)'::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids && ["0440180295"]'::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids(# = "0440180295") '::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids.#($ = "0440180295") '::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids && ["0440180295"] and product_sales_rank > 300000'::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids <@ ["B00000DG0U", "B00004SQXU", "B0001XAM18", "B00000FDBU", "B00000FDBV", "B000002H2H", "B000002H6C", "B000002H5E", "B000002H97", "B000002HMH"]'::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids @> ["B000002H2H", "B000002H6C"]'::jsquery;
select count(*) from test_jsquery where v @@ 'customer_id = null'::jsquery;
select count(*) from test_jsquery where v @@ 'review_votes = true'::jsquery;
select count(*) from test_jsquery where v @@ 'product_group = false'::jsquery;
select count(*) from test_jsquery where v @@ 't = *'::jsquery;
select count(*) from test_jsquery where v @@ 't is boolean'::jsquery;
select count(*) from test_jsquery where v @@ 't is string'::jsquery;
select count(*) from test_jsquery where v @@ 't is numeric'::jsquery;
select count(*) from test_jsquery where v @@ 't is array'::jsquery;
select count(*) from test_jsquery where v @@ 't is object'::jsquery;
select count(*) from test_jsquery where v @@ '$ is boolean'::jsquery;
select count(*) from test_jsquery where v @@ '$ is string'::jsquery;
select count(*) from test_jsquery where v @@ '$ is numeric'::jsquery;
select count(*) from test_jsquery where v @@ '$ is array'::jsquery;
select count(*) from test_jsquery where v @@ '$ is object'::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids.#: is numeric'::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids.#: is string'::jsquery;
select count(*) from test_jsquery where v @@ 'NOT similar_product_ids.#: (NOT $ = "0440180295")'::jsquery;
select count(*) from test_jsquery where v @@ '$ > 2'::jsquery;
select count(*) from test_jsquery where v @@ '$ = false'::jsquery;
select count(*) from test_jsquery where v @@ 't'::jsquery;
select count(*) from test_jsquery where v @@ '$'::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids.#'::jsquery;
select count(*) from test_jsquery where v @@ '$ . ? (review_votes > 10) . review_rating < 7'::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids . ? (# = "B0002W4TL2") . $'::jsquery;

select v from test_jsquery where v @@ 'array <@ [2,3]'::jsquery order by v;
select v from test_jsquery where v @@ 'array && [2,3]'::jsquery order by v;
select v from test_jsquery where v @@ 'array @> [2,3]'::jsquery order by v;
select v from test_jsquery where v @@ 'array = [2,3]'::jsquery order by v;

create index t_idx on test_jsquery using gin (v jsonb_value_path_ops);
set enable_seqscan = off;

explain (costs off) select count(*) from test_jsquery where v @@ 'review_helpful_votes > 0'::jsquery;

select count(*) from test_jsquery where v @@ 'review_helpful_votes > 0'::jsquery;
select count(*) from test_jsquery where v @@ 'review_helpful_votes > 19'::jsquery;
select count(*) from test_jsquery where v @@ 'review_helpful_votes < 19'::jsquery;
select count(*) from test_jsquery where v @@ 'review_helpful_votes >= 19'::jsquery;
select count(*) from test_jsquery where v @@ 'review_helpful_votes <= 19'::jsquery;
select count(*) from test_jsquery where v @@ 'review_helpful_votes = 19'::jsquery;
select count(*) from test_jsquery where v @@ 'review_helpful_votes > 16'::jsquery AND
										v @@ 'review_helpful_votes < 20'::jsquery;
select count(*) from test_jsquery where v @@ 'review_helpful_votes > 16 and review_helpful_votes < 20'::jsquery;
select count(*) from test_jsquery where v @@ 'review_helpful_votes ($ > 16 and $ < 20)'::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids && ["0440180295"]'::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids(# = "0440180295") '::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids.#($ = "0440180295") '::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids && ["0440180295"] and product_sales_rank > 300000'::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids <@ ["B00000DG0U", "B00004SQXU", "B0001XAM18", "B00000FDBU", "B00000FDBV", "B000002H2H", "B000002H6C", "B000002H5E", "B000002H97", "B000002HMH"]'::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids @> ["B000002H2H", "B000002H6C"]'::jsquery;
select count(*) from test_jsquery where v @@ 'customer_id = null'::jsquery;
select count(*) from test_jsquery where v @@ 'review_votes = true'::jsquery;
select count(*) from test_jsquery where v @@ 'product_group = false'::jsquery;
select count(*) from test_jsquery where v @@ 't = *'::jsquery;
select count(*) from test_jsquery where v @@ 't is boolean'::jsquery;
select count(*) from test_jsquery where v @@ 't is string'::jsquery;
select count(*) from test_jsquery where v @@ 't is numeric'::jsquery;
select count(*) from test_jsquery where v @@ 't is array'::jsquery;
select count(*) from test_jsquery where v @@ 't is object'::jsquery;
select count(*) from test_jsquery where v @@ '$ is boolean'::jsquery;
select count(*) from test_jsquery where v @@ '$ is string'::jsquery;
select count(*) from test_jsquery where v @@ '$ is numeric'::jsquery;
select count(*) from test_jsquery where v @@ '$ is array'::jsquery;
select count(*) from test_jsquery where v @@ '$ is object'::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids.#: is numeric'::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids.#: is string'::jsquery;
select count(*) from test_jsquery where v @@ 'NOT similar_product_ids.#: (NOT $ = "0440180295")'::jsquery;
select count(*) from test_jsquery where v @@ '$ > 2'::jsquery;
select count(*) from test_jsquery where v @@ '$ = false'::jsquery;
select count(*) from test_jsquery where v @@ 't'::jsquery;
select count(*) from test_jsquery where v @@ '$'::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids.#'::jsquery;
select count(*) from test_jsquery where v @@ '$ . ? (review_votes > 10) . review_rating < 7'::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids . ? (# = "B0002W4TL2") . $'::jsquery;

explain (costs off) select v from test_jsquery where v @@ 'array <@ [2,3]'::jsquery order by v;
explain (costs off) select v from test_jsquery where v @@ 'array && [2,3]'::jsquery order by v;
explain (costs off) select v from test_jsquery where v @@ 'array @> [2,3]'::jsquery order by v;
explain (costs off) select v from test_jsquery where v @@ 'array = [2,3]'::jsquery order by v;

select v from test_jsquery where v @@ 'array <@ [2,3]'::jsquery order by v;
select v from test_jsquery where v @@ 'array && [2,3]'::jsquery order by v;
select v from test_jsquery where v @@ 'array @> [2,3]'::jsquery order by v;
select v from test_jsquery where v @@ 'array = [2,3]'::jsquery order by v;

explain (costs off) select count(*) from test_jsquery where v @@ '$.review_helpful_votes > 0'::jsonpath;
explain (costs off) select count(*) from test_jsquery where v @? '$.review_helpful_votes ? (@ > 0)'::jsonpath;

select count(*) from test_jsquery where v @@ '$.review_helpful_votes > 0'::jsonpath;
select count(*) from test_jsquery where v @@ '$.review_helpful_votes > 19'::jsonpath;
select count(*) from test_jsquery where v @@ '$.review_helpful_votes < 19'::jsonpath;
select count(*) from test_jsquery where v @@ '$.review_helpful_votes >= 19'::jsonpath;
select count(*) from test_jsquery where v @@ '$.review_helpful_votes <= 19'::jsonpath;
select count(*) from test_jsquery where v @@ '$.review_helpful_votes == 19'::jsonpath;
select count(*) from test_jsquery where v @@ '$.review_helpful_votes > 16'::jsonpath AND
										v @@ '$.review_helpful_votes < 20'::jsonpath;
select count(*) from test_jsquery where v @@ '$.review_helpful_votes > 16 && $.review_helpful_votes < 20'::jsonpath;
select count(*) from test_jsquery where v @? '$.review_helpful_votes ? (@ > 16 && @ < 20)'::jsonpath;
select count(*) from test_jsquery where v @@ '$.similar_product_ids[*] == "0440180295"'::jsonpath;
select count(*) from test_jsquery where v @? '$.similar_product_ids ? (@[*] == "0440180295")'::jsonpath;
select count(*) from test_jsquery where v @? '$.similar_product_ids[*] ? (@ == "0440180295")'::jsonpath;
select count(*) from test_jsquery where v @@ '$.similar_product_ids[*] == "0440180295" && $.product_sales_rank > 300000'::jsonpath;
--select count(*) from test_jsquery where v @@ 'similar_product_ids <@ ["B00000DG0U", "B00004SQXU", "B0001XAM18", "B00000FDBU", "B00000FDBV", "B000002H2H", "B000002H6C", "B000002H5E", "B000002H97", "B000002HMH"]'::jsquery;
select count(*) from test_jsquery where v @? 'strict $.similar_product_ids ? (@[*] == "B000002H2H" && @[*] == "B000002H6C")'::jsonpath;
select count(*) from test_jsquery where v @@ '$.customer_id == null'::jsonpath;
select count(*) from test_jsquery where v @@ '$.review_votes == true'::jsonpath;
select count(*) from test_jsquery where v @@ '$.product_group == false'::jsonpath;
select count(*) from test_jsquery where v @? '$.t'::jsonpath;
select count(*) from test_jsquery where v @@ '$.t.type() == "boolean"'::jsonpath;
select count(*) from test_jsquery where v @@ '$.t.type() == "string"'::jsonpath;
select count(*) from test_jsquery where v @@ '$.t.type() == "number"'::jsonpath;
select count(*) from test_jsquery where v @@ '$.t.type() == "array"'::jsonpath;
select count(*) from test_jsquery where v @@ '$.t.type() == "object"'::jsonpath;
select count(*) from test_jsquery where v @@ '$.type() == "boolean"'::jsonpath;
select count(*) from test_jsquery where v @@ '$.type() == "string"'::jsonpath;
select count(*) from test_jsquery where v @@ '$.type() == "number"'::jsonpath;
select count(*) from test_jsquery where v @@ '$.type() == "array"'::jsonpath;
select count(*) from test_jsquery where v @@ '$.type() == "object"'::jsonpath;
--select count(*) from test_jsquery where v @@ 'similar_product_ids.#: ($ is numeric)'::jsonpath;
--select count(*) from test_jsquery where v @@ 'similar_product_ids.#: ($ is string)'::jsonpath;
--select count(*) from test_jsquery where v @@ 'NOT similar_product_ids.#: (NOT $ = "0440180295")'::jsonpath;
select count(*) from test_jsquery where v @@ 'strict $ > 2'::jsonpath;
select count(*) from test_jsquery where v @@ '$ == false'::jsonpath;
select count(*) from test_jsquery where v @? '$.t'::jsonpath;
select count(*) from test_jsquery where v @? '$'::jsonpath;
select count(*) from test_jsquery where v @? '$.similar_product_ids[*]'::jsonpath;
select count(*) from test_jsquery where v @@ '$ ? (@.review_votes > 10) . review_rating < 7'::jsonpath;
select count(*) from test_jsquery where v @? '$.similar_product_ids ? (@[*] == "B0002W4TL2") '::jsonpath;

--explain (costs off) select v from test_jsquery where v @@ '$.array <@ [2,3]'::jsonpath order by v;
explain (costs off) select v from test_jsquery where v @? '$.array[*] ? (@ == 2 || @ == 3)'::jsonpath order by v;
explain (costs off) select v from test_jsquery where v @@ '$.array[*] == 2 && $.array[*] == 3'::jsonpath order by v;
explain (costs off) select v from test_jsquery where v @@ '$.array[0] == 2 && $.array[1] == 3 && $.array.size() == 2'::jsonpath order by v;

--select v from test_jsquery where v @@ 'array <@ [2,3]'::jsonpath order by v;
select v from test_jsquery where v @? '$.array[*] ? (@ == 2 || @ == 3)'::jsonpath order by v;
select v from test_jsquery where v @@ '$.array[*] == 2 && $.array[*] == 3'::jsonpath order by v;
select v from test_jsquery where v @@ '$.array[0] == 2 && $.array[1] == 3 && $.array.size() == 2'::jsonpath order by v;

drop index t_idx;

create index t_idx on test_jsquery using gin (v jsonb_path_value_ops);
set enable_seqscan = off;

explain (costs off) select count(*) from test_jsquery where v @@ 'review_helpful_votes > 0'::jsquery;

select count(*) from test_jsquery where v @@ 'review_helpful_votes > 0'::jsquery;
select count(*) from test_jsquery where v @@ 'review_helpful_votes > 19'::jsquery;
select count(*) from test_jsquery where v @@ 'review_helpful_votes < 19'::jsquery;
select count(*) from test_jsquery where v @@ 'review_helpful_votes >= 19'::jsquery;
select count(*) from test_jsquery where v @@ 'review_helpful_votes <= 19'::jsquery;
select count(*) from test_jsquery where v @@ 'review_helpful_votes = 19'::jsquery;
select count(*) from test_jsquery where v @@ 'review_helpful_votes > 16'::jsquery AND
										v @@ 'review_helpful_votes < 20'::jsquery;
select count(*) from test_jsquery where v @@ 'review_helpful_votes > 16 and review_helpful_votes < 20'::jsquery;
select count(*) from test_jsquery where v @@ 'review_helpful_votes ($ > 16 and $ < 20)'::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids && ["0440180295"]'::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids(# = "0440180295") '::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids.#($ = "0440180295") '::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids && ["0440180295"] and product_sales_rank > 300000'::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids <@ ["B00000DG0U", "B00004SQXU", "B0001XAM18", "B00000FDBU", "B00000FDBV", "B000002H2H", "B000002H6C", "B000002H5E", "B000002H97", "B000002HMH"]'::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids @> ["B000002H2H", "B000002H6C"]'::jsquery;
select count(*) from test_jsquery where v @@ 'customer_id = null'::jsquery;
select count(*) from test_jsquery where v @@ 'review_votes = true'::jsquery;
select count(*) from test_jsquery where v @@ 'product_group = false'::jsquery;
select count(*) from test_jsquery where v @@ 't = *'::jsquery;
select count(*) from test_jsquery where v @@ 't is boolean'::jsquery;
select count(*) from test_jsquery where v @@ 't is string'::jsquery;
select count(*) from test_jsquery where v @@ 't is numeric'::jsquery;
select count(*) from test_jsquery where v @@ 't is array'::jsquery;
select count(*) from test_jsquery where v @@ 't is object'::jsquery;
select count(*) from test_jsquery where v @@ '$ is boolean'::jsquery;
select count(*) from test_jsquery where v @@ '$ is string'::jsquery;
select count(*) from test_jsquery where v @@ '$ is numeric'::jsquery;
select count(*) from test_jsquery where v @@ '$ is array'::jsquery;
select count(*) from test_jsquery where v @@ '$ is object'::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids.#: is numeric'::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids.#: is string'::jsquery;
select count(*) from test_jsquery where v @@ 'NOT similar_product_ids.#: (NOT $ = "0440180295")'::jsquery;
select count(*) from test_jsquery where v @@ '$ > 2'::jsquery;
select count(*) from test_jsquery where v @@ '$ = false'::jsquery;
select count(*) from test_jsquery where v @@ 't'::jsquery;
select count(*) from test_jsquery where v @@ '$'::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids.#'::jsquery;
select count(*) from test_jsquery where v @@ '$ . ? (review_votes > 10) . review_rating < 7'::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids . ? (# = "B0002W4TL2") . $'::jsquery;

explain (costs off) select v from test_jsquery where v @@ 'array <@ [2,3]'::jsquery order by v;
explain (costs off) select v from test_jsquery where v @@ 'array && [2,3]'::jsquery order by v;
explain (costs off) select v from test_jsquery where v @@ 'array @> [2,3]'::jsquery order by v;
explain (costs off) select v from test_jsquery where v @@ 'array = [2,3]'::jsquery order by v;

select v from test_jsquery where v @@ 'array <@ [2,3]'::jsquery order by v;
select v from test_jsquery where v @@ 'array && [2,3]'::jsquery order by v;
select v from test_jsquery where v @@ 'array @> [2,3]'::jsquery order by v;
select v from test_jsquery where v @@ 'array = [2,3]'::jsquery order by v;

explain (costs off) select count(*) from test_jsquery where v @@ '$.review_helpful_votes > 0'::jsonpath;
explain (costs off) select count(*) from test_jsquery where v @? '$.review_helpful_votes ? (@ > 0)'::jsonpath;

select count(*) from test_jsquery where v @@ '$.review_helpful_votes > 0'::jsonpath;
select count(*) from test_jsquery where v @@ '$.review_helpful_votes > 19'::jsonpath;
select count(*) from test_jsquery where v @@ '$.review_helpful_votes < 19'::jsonpath;
select count(*) from test_jsquery where v @@ '$.review_helpful_votes >= 19'::jsonpath;
select count(*) from test_jsquery where v @@ '$.review_helpful_votes <= 19'::jsonpath;
select count(*) from test_jsquery where v @@ '$.review_helpful_votes == 19'::jsonpath;
select count(*) from test_jsquery where v @@ '$.review_helpful_votes > 16'::jsonpath AND
										v @@ '$.review_helpful_votes < 20'::jsonpath;
select count(*) from test_jsquery where v @@ '$.review_helpful_votes > 16 && $.review_helpful_votes < 20'::jsonpath;
select count(*) from test_jsquery where v @? '$.review_helpful_votes ? (@ > 16 && @ < 20)'::jsonpath;
select count(*) from test_jsquery where v @@ '$.similar_product_ids[*] == "0440180295"'::jsonpath;
select count(*) from test_jsquery where v @? '$.similar_product_ids ? (@[*] == "0440180295")'::jsonpath;
select count(*) from test_jsquery where v @? '$.similar_product_ids[*] ? (@ == "0440180295")'::jsonpath;
select count(*) from test_jsquery where v @@ '$.similar_product_ids[*] == "0440180295" && $.product_sales_rank > 300000'::jsonpath;
--select count(*) from test_jsquery where v @@ 'similar_product_ids <@ ["B00000DG0U", "B00004SQXU", "B0001XAM18", "B00000FDBU", "B00000FDBV", "B000002H2H", "B000002H6C", "B000002H5E", "B000002H97", "B000002HMH"]'::jsquery;
select count(*) from test_jsquery where v @? 'strict $.similar_product_ids ? (@[*] == "B000002H2H" && @[*] == "B000002H6C")'::jsonpath;
select count(*) from test_jsquery where v @@ '$.customer_id == null'::jsonpath;
select count(*) from test_jsquery where v @@ '$.review_votes == true'::jsonpath;
select count(*) from test_jsquery where v @@ '$.product_group == false'::jsonpath;
select count(*) from test_jsquery where v @? '$.t'::jsonpath;
select count(*) from test_jsquery where v @@ '$.t.type() == "boolean"'::jsonpath;
select count(*) from test_jsquery where v @@ '$.t.type() == "string"'::jsonpath;
select count(*) from test_jsquery where v @@ '$.t.type() == "number"'::jsonpath;
select count(*) from test_jsquery where v @@ '$.t.type() == "array"'::jsonpath;
select count(*) from test_jsquery where v @@ '$.t.type() == "object"'::jsonpath;
select count(*) from test_jsquery where v @@ '$.type() == "boolean"'::jsonpath;
select count(*) from test_jsquery where v @@ '$.type() == "string"'::jsonpath;
select count(*) from test_jsquery where v @@ '$.type() == "number"'::jsonpath;
select count(*) from test_jsquery where v @@ '$.type() == "array"'::jsonpath;
select count(*) from test_jsquery where v @@ '$.type() == "object"'::jsonpath;
--select count(*) from test_jsquery where v @@ 'similar_product_ids.#: ($ is numeric)'::jsonpath;
--select count(*) from test_jsquery where v @@ 'similar_product_ids.#: ($ is string)'::jsonpath;
--select count(*) from test_jsquery where v @@ 'NOT similar_product_ids.#: (NOT $ = "0440180295")'::jsonpath;
select count(*) from test_jsquery where v @@ 'strict $ > 2'::jsonpath;
select count(*) from test_jsquery where v @@ '$ == false'::jsonpath;
select count(*) from test_jsquery where v @? '$.t'::jsonpath;
select count(*) from test_jsquery where v @? '$'::jsonpath;
select count(*) from test_jsquery where v @? '$.similar_product_ids[*]'::jsonpath;
select count(*) from test_jsquery where v @@ '$ ? (@.review_votes > 10) . review_rating < 7'::jsonpath;
select count(*) from test_jsquery where v @? '$.similar_product_ids ? (@[*] == "B0002W4TL2") '::jsonpath;

--explain (costs off) select v from test_jsquery where v @@ '$.array <@ [2,3]'::jsonpath order by v;
explain (costs off) select v from test_jsquery where v @? '$.array[*] ? (@ == 2 || @ == 3)'::jsonpath order by v;
explain (costs off) select v from test_jsquery where v @@ '$.array[*] == 2 && $.array[*] == 3'::jsonpath order by v;
explain (costs off) select v from test_jsquery where v @@ '$.array[0] == 2 && $.array[1] == 3 && $.array.size() == 2'::jsonpath order by v;

--select v from test_jsquery where v @@ 'array <@ [2,3]'::jsonpath order by v;
select v from test_jsquery where v @? '$.array[*] ? (@ == 2 || @ == 3)'::jsonpath order by v;
select v from test_jsquery where v @@ '$.array[*] == 2 && $.array[*] == 3'::jsonpath order by v;
select v from test_jsquery where v @@ '$.array[0] == 2 && $.array[1] == 3 && $.array.size() == 2'::jsonpath order by v;

drop index t_idx;

create index t_idx on test_jsquery using gin (v jsonb_laxpath_value_ops);
set enable_seqscan = off;

explain (costs off) select count(*) from test_jsquery where v @@ 'review_helpful_votes > 0'::jsquery;

select count(*) from test_jsquery where v @@ 'review_helpful_votes > 0'::jsquery;
select count(*) from test_jsquery where v @@ 'review_helpful_votes > 19'::jsquery;
select count(*) from test_jsquery where v @@ 'review_helpful_votes < 19'::jsquery;
select count(*) from test_jsquery where v @@ 'review_helpful_votes >= 19'::jsquery;
select count(*) from test_jsquery where v @@ 'review_helpful_votes <= 19'::jsquery;
select count(*) from test_jsquery where v @@ 'review_helpful_votes = 19'::jsquery;
select count(*) from test_jsquery where v @@ 'review_helpful_votes > 16'::jsquery AND
										v @@ 'review_helpful_votes < 20'::jsquery;
select count(*) from test_jsquery where v @@ 'review_helpful_votes > 16 and review_helpful_votes < 20'::jsquery;
select count(*) from test_jsquery where v @@ 'review_helpful_votes ($ > 16 and $ < 20)'::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids && ["0440180295"]'::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids(# = "0440180295") '::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids.#($ = "0440180295") '::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids && ["0440180295"] and product_sales_rank > 300000'::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids <@ ["B00000DG0U", "B00004SQXU", "B0001XAM18", "B00000FDBU", "B00000FDBV", "B000002H2H", "B000002H6C", "B000002H5E", "B000002H97", "B000002HMH"]'::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids @> ["B000002H2H", "B000002H6C"]'::jsquery;
select count(*) from test_jsquery where v @@ 'customer_id = null'::jsquery;
select count(*) from test_jsquery where v @@ 'review_votes = true'::jsquery;
select count(*) from test_jsquery where v @@ 'product_group = false'::jsquery;
select count(*) from test_jsquery where v @@ 't = *'::jsquery;
select count(*) from test_jsquery where v @@ 't is boolean'::jsquery;
select count(*) from test_jsquery where v @@ 't is string'::jsquery;
select count(*) from test_jsquery where v @@ 't is numeric'::jsquery;
select count(*) from test_jsquery where v @@ 't is array'::jsquery;
select count(*) from test_jsquery where v @@ 't is object'::jsquery;
select count(*) from test_jsquery where v @@ '$ is boolean'::jsquery;
select count(*) from test_jsquery where v @@ '$ is string'::jsquery;
select count(*) from test_jsquery where v @@ '$ is numeric'::jsquery;
select count(*) from test_jsquery where v @@ '$ is array'::jsquery;
select count(*) from test_jsquery where v @@ '$ is object'::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids.#: is numeric'::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids.#: is string'::jsquery;
select count(*) from test_jsquery where v @@ 'NOT similar_product_ids.#: (NOT $ = "0440180295")'::jsquery;
select count(*) from test_jsquery where v @@ '$ > 2'::jsquery;
select count(*) from test_jsquery where v @@ '$ = false'::jsquery;
select count(*) from test_jsquery where v @@ 't'::jsquery;
select count(*) from test_jsquery where v @@ '$'::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids.#'::jsquery;
select count(*) from test_jsquery where v @@ '$ . ? (review_votes > 10) . review_rating < 7'::jsquery;
select count(*) from test_jsquery where v @@ 'similar_product_ids . ? (# = "B0002W4TL2") . $'::jsquery;

explain (costs off) select v from test_jsquery where v @@ 'array <@ [2,3]'::jsquery order by v;
explain (costs off) select v from test_jsquery where v @@ 'array && [2,3]'::jsquery order by v;
explain (costs off) select v from test_jsquery where v @@ 'array @> [2,3]'::jsquery order by v;
explain (costs off) select v from test_jsquery where v @@ 'array = [2,3]'::jsquery order by v;

select v from test_jsquery where v @@ 'array <@ [2,3]'::jsquery order by v;
select v from test_jsquery where v @@ 'array && [2,3]'::jsquery order by v;
select v from test_jsquery where v @@ 'array @> [2,3]'::jsquery order by v;
select v from test_jsquery where v @@ 'array = [2,3]'::jsquery order by v;


explain (costs off) select count(*) from test_jsquery where v @@ '$.review_helpful_votes > 0'::jsonpath;
explain (costs off) select count(*) from test_jsquery where v @? '$.review_helpful_votes ? (@ > 0)'::jsonpath;

select count(*) from test_jsquery where v @@ '$.review_helpful_votes > 0'::jsonpath;
select count(*) from test_jsquery where v @@ '$.review_helpful_votes > 19'::jsonpath;
select count(*) from test_jsquery where v @@ '$.review_helpful_votes < 19'::jsonpath;
select count(*) from test_jsquery where v @@ '$.review_helpful_votes >= 19'::jsonpath;
select count(*) from test_jsquery where v @@ '$.review_helpful_votes <= 19'::jsonpath;
select count(*) from test_jsquery where v @@ '$.review_helpful_votes == 19'::jsonpath;
select count(*) from test_jsquery where v @@ '$.review_helpful_votes > 16'::jsonpath AND
										v @@ '$.review_helpful_votes < 20'::jsonpath;
select count(*) from test_jsquery where v @@ '$.review_helpful_votes > 16 && $.review_helpful_votes < 20'::jsonpath;
select count(*) from test_jsquery where v @? '$.review_helpful_votes ? (@ > 16 && @ < 20)'::jsonpath;
select count(*) from test_jsquery where v @@ '$.similar_product_ids[*] == "0440180295"'::jsonpath;
select count(*) from test_jsquery where v @? '$.similar_product_ids ? (@[*] == "0440180295")'::jsonpath;
select count(*) from test_jsquery where v @? '$.similar_product_ids[*] ? (@ == "0440180295")'::jsonpath;
select count(*) from test_jsquery where v @@ '$.similar_product_ids[*] == "0440180295" && $.product_sales_rank > 300000'::jsonpath;
--select count(*) from test_jsquery where v @@ 'similar_product_ids <@ ["B00000DG0U", "B00004SQXU", "B0001XAM18", "B00000FDBU", "B00000FDBV", "B000002H2H", "B000002H6C", "B000002H5E", "B000002H97", "B000002HMH"]'::jsquery;
select count(*) from test_jsquery where v @? 'strict $.similar_product_ids ? (@[*] == "B000002H2H" && @[*] == "B000002H6C")'::jsonpath;
select count(*) from test_jsquery where v @@ '$.customer_id == null'::jsonpath;
select count(*) from test_jsquery where v @@ '$.review_votes == true'::jsonpath;
select count(*) from test_jsquery where v @@ '$.product_group == false'::jsonpath;
select count(*) from test_jsquery where v @? '$.t'::jsonpath;
select count(*) from test_jsquery where v @@ '$.t.type() == "boolean"'::jsonpath;
select count(*) from test_jsquery where v @@ '$.t.type() == "string"'::jsonpath;
select count(*) from test_jsquery where v @@ '$.t.type() == "number"'::jsonpath;
select count(*) from test_jsquery where v @@ '$.t.type() == "array"'::jsonpath;
select count(*) from test_jsquery where v @@ '$.t.type() == "object"'::jsonpath;
select count(*) from test_jsquery where v @@ '$.type() == "boolean"'::jsonpath;
select count(*) from test_jsquery where v @@ '$.type() == "string"'::jsonpath;
select count(*) from test_jsquery where v @@ '$.type() == "number"'::jsonpath;
select count(*) from test_jsquery where v @@ '$.type() == "array"'::jsonpath;
select count(*) from test_jsquery where v @@ '$.type() == "object"'::jsonpath;
--select count(*) from test_jsquery where v @@ 'similar_product_ids.#: ($ is numeric)'::jsonpath;
--select count(*) from test_jsquery where v @@ 'similar_product_ids.#: ($ is string)'::jsonpath;
--select count(*) from test_jsquery where v @@ 'NOT similar_product_ids.#: (NOT $ = "0440180295")'::jsonpath;
select count(*) from test_jsquery where v @@ 'strict $ > 2'::jsonpath;
select count(*) from test_jsquery where v @@ '$ == false'::jsonpath;
select count(*) from test_jsquery where v @? '$.t'::jsonpath;
select count(*) from test_jsquery where v @? '$'::jsonpath;
select count(*) from test_jsquery where v @? '$.similar_product_ids[*]'::jsonpath;
select count(*) from test_jsquery where v @@ '$ ? (@.review_votes > 10) . review_rating < 7'::jsonpath;
select count(*) from test_jsquery where v @? '$.similar_product_ids ? (@[*] == "B0002W4TL2") '::jsonpath;

--explain (costs off) select v from test_jsquery where v @@ '$.array <@ [2,3]'::jsonpath order by v;
explain (costs off) select v from test_jsquery where v @? '$.array[*] ? (@ == 2 || @ == 3)'::jsonpath order by v;
explain (costs off) select v from test_jsquery where v @@ '$.array[*] == 2 && $.array[*] == 3'::jsonpath order by v;
explain (costs off) select v from test_jsquery where v @@ '$.array[0] == 2 && $.array[1] == 3 && $.array.size() == 2'::jsonpath order by v;

--select v from test_jsquery where v @@ 'array <@ [2,3]'::jsonpath order by v;
select v from test_jsquery where v @? '$.array[*] ? (@ == 2 || @ == 3)'::jsonpath order by v;
select v from test_jsquery where v @@ '$.array[*] == 2 && $.array[*] == 3'::jsonpath order by v;
select v from test_jsquery where v @@ '$.array[0] == 2 && $.array[1] == 3 && $.array.size() == 2'::jsonpath order by v;

RESET enable_seqscan;
