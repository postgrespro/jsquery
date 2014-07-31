CREATE EXTENSION jsquery;

set escape_string_warning=off;

CREATE TABLE test_jsquery (v jsonb);

\copy test_jsquery from 'data/test_jsquery.data'

select 'asd.zzz = 13'::jsquery;
select 'asd.zzz < 13'::jsquery;
select 'asd(zzz < 13)'::jsquery;
select 'asd(zzz < 13 & x = true)'::jsquery;
select 'asd(zzz < 13 & x = "true")'::jsquery;
select 'asd(zzz < 13 & x.zxc = "true")'::jsquery;
select 'asd(zzz < 13 | #.zxc = "true")'::jsquery;
select 'asd(* < 13 & #.zxc = "true")'::jsquery;
select '(* < 13 & #.zxc = "true")'::jsquery;
select '* < 13 & #.zxc = "true"'::jsquery;

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

select '{"a": {"b": null}}'::jsonb @@ 'a.b = 1';
select '{"a": {"b": null}}'::jsonb @@ 'a.b = null';
select '{"a": {"b": null}}'::jsonb @@ 'a.b = false';
select '{"a": {"b": false}}'::jsonb @@ 'a.b = false';
select '{"a": {"b": false}}'::jsonb @@ 'a.b = true';
select '{"a": {"b": true}}'::jsonb @@ 'a.b = true';


select '{"a": {"b": 1}}'::jsonb @@ 'a.b = 1';
select '{"a": {"b": 1}}'::jsonb @@ 'a.b < 1';
select '{"a": {"b": 1}}'::jsonb @@ 'a.b <= 1';
select '{"a": {"b": 1}}'::jsonb @@ 'a.b >= 1';
select '{"a": {"b": 1}}'::jsonb @@ 'a.b > 1';

select '{"a": {"b": 1}}'::jsonb @@ 'a.b = 2';
select '{"a": {"b": 1}}'::jsonb @@ 'a.b < 2';
select '{"a": {"b": 1}}'::jsonb @@ 'a.b <= 2';
select '{"a": {"b": 1}}'::jsonb @@ 'a.b >= 2';
select '{"a": {"b": 1}}'::jsonb @@ 'a.b > 2';

select '{"a": {"b": 1}}'::jsonb @@ 'a.b = 0';
select '{"a": {"b": 1}}'::jsonb @@ 'a.b < 0';
select '{"a": {"b": 1}}'::jsonb @@ 'a.b <= 0';
select '{"a": {"b": 1}}'::jsonb @@ 'a.b >= 0';
select '{"a": {"b": 1}}'::jsonb @@ 'a.b > 0';

select '{"a": {"b": 1}}'::jsonb @@ '*.b > 0';
select '{"a": {"b": 1}}'::jsonb @@ '*.b > 0';
select '{"a": {"b": 1}}'::jsonb @@ 'a.* > 0';
select '{"a": {"b": 1}}'::jsonb @@ 'a.* > 0';

select '{"a": {"b": [1,2,3]}}'::jsonb @@ '*.b && [ 1 ]';
select '{"a": {"b": [1,2,3]}}'::jsonb @@ '*.b @> [ 1 ]';
select '{"a": {"b": [1,2,3]}}'::jsonb @@ '*.b <@ [ 1 ]';
select '{"a": {"b": [1,2,3]}}'::jsonb @@ '*.b @> [ 1,2,3,4 ]';
select '{"a": {"b": [1,2,3]}}'::jsonb @@ '*.b <@ [ 1,2,3,4 ]';

select '[{"a": 2}, {"a": 3}]'::jsonb @@ '*.a = 4';
select '[{"a": 2}, {"a": 3}]'::jsonb @@ '*.a = 3';

select '[{"a": 2}, {"a": 3}, {"a": {"a":4}}]'::jsonb @@ '#.a = 4';
select '[{"a": 2}, {"a": 3}, {"a": {"a":4}}]'::jsonb @@ '*.a = 4';

select '[{"a": 2}, {"a": 3}, {"a": {"a":4}}]'::jsonb @@ '#(a = 1 | a=3)';
select '[{"a": 2}, {"a": 3}, {"a": {"a":4}}]'::jsonb @@ '#(a = 3 | a=1)';
select '[{"a": 2}, {"a": 3}, {"a": {"a":4}}]'::jsonb @@ '#(a = 3 & a=1)';
select '[{"a": 2}, {"a": 3}, {"a": {"a":4}}]'::jsonb @@ '#(a = 3 & a=2)' as "false";
select '[{"a": 2, "b":3}, {"a": 3, "b": 1}]'::jsonb @@ '#(b = 1 & a=3)';

select '[{"a": 2}, {"a": 3}, {"a": {"a":4}}]'::jsonb @@ '#.a.a = 4';
select '[{"a": 2}, {"a": 3}, {"a": {"a":4}}]'::jsonb @@ '*.a.a = 4';
select '[{"a": 2}, {"a": 3}, {"a": {"a":4}}]'::jsonb @@ '*.#.a.a = 4';
select '[{"a": 2}, {"a": 3}, {"a": {"a":4}}]'::jsonb @@ '#.*.a.a = 4';

select '{"a": 1}'::jsonb @@ 'a in (0,1,2)';
select '{"a": 1}'::jsonb @@ 'a in (0,2)';

select '{"a": {"b": [1,2,3]}}'::jsonb @@ 'a.b.#=2';
select '{"a": {"b": [1,2,3]}}'::jsonb @@ '*.b && [ 5 ]';

select '{"a": {"b": [1,2,3]}}'::jsonb @@ 'a=*';
select '{"a": {"b": [1,2,3]}}'::jsonb @@ 'a.b=*';
select '{"a": {"b": [1,2,3]}}'::jsonb @@ 'a.c=*';

select '{"a": {"b": [1,2,3]}}'::jsonb @@ 'a.b = [1,2,3]';
select '{"a": {"b": [1,2,3]}}'::jsonb @@ 'a.b.# = [1,2,3]';
select '{"a": {"b": [1,2,3]}}'::jsonb @@ 'a.b && [1,2,3]';
select '{"a": {"b": [1,2,3]}}'::jsonb @@ 'a.b.# && [1,2,3]';

select 'asd.# = 3'::jsquery & 'zzz = true' | 'xxx.# = zero';
select !'asd.# = 3'::jsquery & 'zzz = true' | !'xxx.# = zero';

select '{"x":[0,1,1,2]}'::jsonb @@ 'x @> [1,0]'::jsquery;
select '{"x":[0,1,1,2]}'::jsonb @@ 'x @> [1,0,1]'::jsquery;
select '{"x":[0,1,1,2]}'::jsonb @@ 'x @> [1,0,3]'::jsquery;

select '{"a": {"b": [1,2,3]}}'::jsonb @@ '*.b && [ 2 ]';
select '{"a": {"b": [1,2,3]}}'::jsonb @@ '*.b($ && [ 2 ])';
select '{"a": {"b": [1,2,3]}}'::jsonb @@ 'a.$.b && [ 2 ]';
select '{"a": {"b": [1,2,3]}}'::jsonb @@ 'a.$.b ($ && [ 2 ])';

select '[1,2,3]'::jsonb @@ '# && [2]';
select '[1,2,3]'::jsonb @@ '#($ && [2])';
select '[1,2,3]'::jsonb @@ '$ && [2]';
select '[1,2,3]'::jsonb @@ '$ ($ && [2])';
select '[1,2,3]'::jsonb @@ '$ = 2';
select '[1,2,3]'::jsonb @@ '# = 2';
select '[1,2,3]'::jsonb @@ '#.$ = 2';
select '[1,2,3]'::jsonb @@ '#($ = 2)';

select '{"a": {"b": 3, "c": "hey"}, "x": [5,6]}'::jsonb @@ '%.b=3';
select '{"a": {"b": 3, "c": "hey"}, "x": [5,6]}'::jsonb @@ 'a.%=3';
select '{"a": {"b": 3, "c": "hey"}, "x": [5,6]}'::jsonb @@ '%.%="hey"';
select '{"a": {"b": 3, "c": "hey"}, "x": [5,6]}'::jsonb @@ '%="hey"';
select '{"a": {"b": 3, "c": "hey"}, "x": [5,6]}'::jsonb @@ '%=[5,6]';

select '"XXX"'::jsonb @@ '$="XXX"';
select '"XXX"'::jsonb @@ '#.$="XXX"';

--Unicode

select 'a\t = "dollar \u0024 character"'::jsquery;
select '{ "a":  "dollar \u0024 character" }'::jsonb @@ '* = "dollar \u0024 character"';
select '{ "a":  "dollar \u0024 character" }'::jsonb @@ '* = "dollar $ character"';
select '{ "a":  "dollar $ character" }'::jsonb @@ '* = "dollar \u0024 character"';
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

select  'as IS boolean | as is ARRAY | as is ObJect | as is Number | as is string'::jsquery;
select '{"as": "xxx"}' @@ 'as IS string'::jsquery;
select '{"as": "xxx"}' @@ 'as IS boolean | as is ARRAY | as is ObJect | as is Number'::jsquery;
select '{"as": 5}' @@ 'as is Number'::jsquery;
select '{"as": true}' @@ 'as is boolean'::jsquery;
select '{"as": false}' @@ 'as is boolean'::jsquery;
select '{"as": "false"}' @@ 'as is boolean'::jsquery;
select '["xxx"]' @@ '$ IS array'::jsquery;
select '{"as": false}' @@ '$ IS object'::jsquery;
select '"xxx"' @@ '$ IS string'::jsquery;
select '"xxx"' @@ '$ IS number'::jsquery;

---table and index

select count(*) from test_jsquery where (v->>'review_helpful_votes')::int4 > 0;
select count(*) from test_jsquery where (v->>'review_helpful_votes')::int4 > 19;
select count(*) from test_jsquery where (v->>'review_helpful_votes')::int4 < 19;
select count(*) from test_jsquery where (v->>'review_helpful_votes')::int4 >= 19;
select count(*) from test_jsquery where (v->>'review_helpful_votes')::int4 <= 19;
select count(*) from test_jsquery where (v->>'review_helpful_votes')::int4 = 19;
select count(*) from test_jsquery where (v->>'review_helpful_votes')::int4 > 16 AND
										(v->>'review_helpful_votes')::int4 < 20;


select count(*) from test_jsquery where v @@ 'review_helpful_votes > 0';
select count(*) from test_jsquery where v @@ 'review_helpful_votes > 19';
select count(*) from test_jsquery where v @@ 'review_helpful_votes < 19';
select count(*) from test_jsquery where v @@ 'review_helpful_votes >= 19';
select count(*) from test_jsquery where v @@ 'review_helpful_votes <= 19';
select count(*) from test_jsquery where v @@ 'review_helpful_votes = 19';
select count(*) from test_jsquery where v @@ 'review_helpful_votes > 16' AND
										v @@ 'review_helpful_votes < 20';
select count(*) from test_jsquery where v @@ 'review_helpful_votes > 16 & review_helpful_votes < 20';
select count(*) from test_jsquery where v @@ 'review_helpful_votes ($ > 16 & $ < 20)';
select count(*) from test_jsquery where v @@ 'similar_product_ids && ["0440180295"]';
select count(*) from test_jsquery where v @@ 'similar_product_ids(# = "0440180295") ';
select count(*) from test_jsquery where v @@ 'similar_product_ids.#($ = "0440180295") ';
select count(*) from test_jsquery where v @@ 'similar_product_ids && ["0440180295"] & product_sales_rank > 300000';
select count(*) from test_jsquery where v @@ 'customer_id = null';
select count(*) from test_jsquery where v @@ 'review_votes = true';
select count(*) from test_jsquery where v @@ 'product_group = false';

create index t_idx on test_jsquery using gin (v jsonb_value_path_ops);
set enable_seqscan = off;

explain (costs off) select count(*) from test_jsquery where v @@ 'review_helpful_votes > 0';

select count(*) from test_jsquery where v @@ 'review_helpful_votes > 0';
select count(*) from test_jsquery where v @@ 'review_helpful_votes > 19';
select count(*) from test_jsquery where v @@ 'review_helpful_votes < 19';
select count(*) from test_jsquery where v @@ 'review_helpful_votes >= 19';
select count(*) from test_jsquery where v @@ 'review_helpful_votes <= 19';
select count(*) from test_jsquery where v @@ 'review_helpful_votes = 19';
select count(*) from test_jsquery where v @@ 'review_helpful_votes > 16' AND
										v @@ 'review_helpful_votes < 20';
select count(*) from test_jsquery where v @@ 'review_helpful_votes > 16 & review_helpful_votes < 20';
select count(*) from test_jsquery where v @@ 'review_helpful_votes ($ > 16 & $ < 20)';
select count(*) from test_jsquery where v @@ 'similar_product_ids && ["0440180295"]';
select count(*) from test_jsquery where v @@ 'similar_product_ids(# = "0440180295") ';
select count(*) from test_jsquery where v @@ 'similar_product_ids.#($ = "0440180295") ';
select count(*) from test_jsquery where v @@ 'similar_product_ids && ["0440180295"] & product_sales_rank > 300000';
select count(*) from test_jsquery where v @@ 'customer_id = null';
select count(*) from test_jsquery where v @@ 'review_votes = true';
select count(*) from test_jsquery where v @@ 'product_group = false';

drop index t_idx;

create index t_idx on test_jsquery using gin (v jsonb_path_value_ops);
set enable_seqscan = off;

explain (costs off) select count(*) from test_jsquery where v @@ 'review_helpful_votes > 0';

select count(*) from test_jsquery where v @@ 'review_helpful_votes > 0';
select count(*) from test_jsquery where v @@ 'review_helpful_votes > 19';
select count(*) from test_jsquery where v @@ 'review_helpful_votes < 19';
select count(*) from test_jsquery where v @@ 'review_helpful_votes >= 19';
select count(*) from test_jsquery where v @@ 'review_helpful_votes <= 19';
select count(*) from test_jsquery where v @@ 'review_helpful_votes = 19';
select count(*) from test_jsquery where v @@ 'review_helpful_votes > 16' AND
										v @@ 'review_helpful_votes < 20';
select count(*) from test_jsquery where v @@ 'review_helpful_votes > 16 & review_helpful_votes < 20';
select count(*) from test_jsquery where v @@ 'review_helpful_votes ($ > 16 & $ < 20)';
select count(*) from test_jsquery where v @@ 'similar_product_ids && ["0440180295"]';
select count(*) from test_jsquery where v @@ 'similar_product_ids(# = "0440180295") ';
select count(*) from test_jsquery where v @@ 'similar_product_ids.#($ = "0440180295") ';
select count(*) from test_jsquery where v @@ 'similar_product_ids && ["0440180295"] & product_sales_rank > 300000';
select count(*) from test_jsquery where v @@ 'customer_id = null';
select count(*) from test_jsquery where v @@ 'review_votes = true';
select count(*) from test_jsquery where v @@ 'product_group = false';

RESET enable_seqscan;
