DROP FUNCTION IF EXISTS explain_jsonb(text);

CREATE OR REPLACE FUNCTION explain_jsonb(sql_query text)
RETURNS TABLE(explain_line json) AS
$$
BEGIN
	RETURN QUERY EXECUTE 'EXPLAIN (ANALYZE, FORMAT json) ' || sql_query;
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION get_plan_and_actual_rows(sql_query text)
RETURNS TABLE(plan integer, actual integer) AS
$$
	SELECT
		(plan->>'Plan Rows')::integer plan,
		(plan->>'Actual Rows')::integer actual
	FROM (
		SELECT explain_jsonb(sql_query) #> '{0,Plan,Plans,0}'
	) p(plan)
$$ LANGUAGE sql;

CREATE OR REPLACE FUNCTION check_estimate(sql_query text, accuracy real)
RETURNS boolean AS
$$
	SELECT plan BETWEEN actual / (1 + accuracy) AND (actual + 1) * (1 + accuracy)
	FROM (SELECT * FROM get_plan_and_actual_rows(sql_query)) x
$$ LANGUAGE sql;

CREATE OR REPLACE FUNCTION check_estimate2(sql_query text, accuracy real)
RETURNS TABLE(min integer, max integer) AS
$$
	SELECT (actual * (1 - accuracy))::integer, ((actual + 1) * (1 + accuracy))::integer
	FROM (SELECT * FROM get_plan_and_actual_rows(sql_query)) x
$$ LANGUAGE sql;

DROP TABLE IF EXISTS jsonb_stats_test;
CREATE TABLE jsonb_stats_test(js jsonb);

INSERT INTO jsonb_stats_test SELECT NULL FROM generate_series(1, 10000);

INSERT INTO jsonb_stats_test SELECT 'null' FROM generate_series(1, 2000);
INSERT INTO jsonb_stats_test SELECT 'true' FROM generate_series(1, 3000);
INSERT INTO jsonb_stats_test SELECT 'false' FROM generate_series(1, 5000);

INSERT INTO jsonb_stats_test SELECT '12345' FROM generate_series(1, 1000);
INSERT INTO jsonb_stats_test SELECT (1000 * (i % 10))::text::jsonb FROM generate_series(1, 4000) i;
INSERT INTO jsonb_stats_test SELECT i::text::jsonb FROM generate_series(1, 5000) i;

INSERT INTO jsonb_stats_test SELECT '"foo"' FROM generate_series(1, 1000);
INSERT INTO jsonb_stats_test SELECT format('"bar%s"', i % 10)::jsonb FROM generate_series(1, 4000) i;
INSERT INTO jsonb_stats_test SELECT format('"baz%s"', i)::jsonb FROM generate_series(1, 5000) i;

INSERT INTO jsonb_stats_test SELECT '{}' FROM generate_series(1, 1000);
INSERT INTO jsonb_stats_test SELECT jsonb_build_object('foo', 'bar') FROM generate_series(1, 1000);
INSERT INTO jsonb_stats_test SELECT jsonb_build_object('foo', 'baz' || (i % 10)) FROM generate_series(1, 3000) i;
INSERT INTO jsonb_stats_test SELECT jsonb_build_object('foo', i % 10) FROM generate_series(1, 2000) i;
INSERT INTO jsonb_stats_test SELECT jsonb_build_object('"foo \"bar"', i % 10) FROM generate_series(1, 2000) i;

INSERT INTO jsonb_stats_test SELECT '[]' FROM generate_series(1, 1000);
INSERT INTO jsonb_stats_test SELECT '["foo"]' FROM generate_series(1, 2000);
INSERT INTO jsonb_stats_test SELECT '[12345]' FROM generate_series(1, 3000);
INSERT INTO jsonb_stats_test SELECT '[["foo"]]' FROM generate_series(1, 2000);
INSERT INTO jsonb_stats_test SELECT '[{"key": "foo"}]' FROM generate_series(1, 2000);
INSERT INTO jsonb_stats_test SELECT '[null, "foo"]' FROM generate_series(1, 2000);
INSERT INTO jsonb_stats_test SELECT '[null, 12345]' FROM generate_series(1, 3000);
INSERT INTO jsonb_stats_test SELECT '[null, ["foo"]]' FROM generate_series(1, 2000);
INSERT INTO jsonb_stats_test SELECT '[null, {"key": "foo"}]' FROM generate_series(1, 2000);

-- Build random variable-length integer arrays
INSERT INTO jsonb_stats_test
SELECT jsonb_build_object('array',
	jsonb_build_array())
FROM generate_series(1, 1000);

INSERT INTO jsonb_stats_test
SELECT jsonb_build_object('array',
	jsonb_build_array(
		floor(random() * 10)::int))
FROM generate_series(1, 4000);

INSERT INTO jsonb_stats_test
SELECT jsonb_build_object('array',
	jsonb_build_array(
		floor(random() * 10)::int,
		floor(random() * 10)::int))
FROM generate_series(1, 3000);

INSERT INTO jsonb_stats_test
SELECT jsonb_build_object('array',
	jsonb_build_array(
		floor(random() * 10)::int,
		floor(random() * 10)::int,
		floor(random() * 10)::int))
FROM generate_series(1, 2000);


ANALYZE jsonb_stats_test;

CREATE OR REPLACE FUNCTION check_jsonb_stats_test_estimate(sql_condition text, accuracy real)
RETURNS boolean AS
$$
	SELECT check_estimate('SELECT count(*) FROM jsonb_stats_test WHERE ' || sql_condition, accuracy)
$$ LANGUAGE sql;

DROP FUNCTION IF EXISTS check_jsonb_stats_test_estimate2(text, real);

CREATE OR REPLACE FUNCTION check_jsonb_stats_test_estimate2(sql_condition text, accuracy real)
RETURNS TABLE(plan integer, actual integer) AS
$$
	SELECT get_plan_and_actual_rows('SELECT count(*) FROM jsonb_stats_test WHERE ' || sql_condition)
$$ LANGUAGE sql;


SELECT check_jsonb_stats_test_estimate($$js @@ '$ = *'$$, 0.01);
SELECT check_jsonb_stats_test_estimate($$js @@ '* = *'$$, 0.01);
SELECT check_jsonb_stats_test_estimate2($$js @@ '% = *'$$, 0.01);

SELECT check_jsonb_stats_test_estimate($$js @@ '$ = null'$$, 0.1);
SELECT check_jsonb_stats_test_estimate($$js @@ '$ = true'$$, 0.1);
SELECT check_jsonb_stats_test_estimate($$js @@ '$ = false'$$, 0.1);

SELECT check_jsonb_stats_test_estimate($$js @@ '$ IS BOOLEAN'$$, 0.1);
SELECT check_jsonb_stats_test_estimate($$js @@ '$ IS NUMERIC'$$, 0.1);
SELECT check_jsonb_stats_test_estimate($$js @@ '$ IS STRING'$$, 0.1);
SELECT check_jsonb_stats_test_estimate($$js @@ '$ IS OBJECT'$$, 0.1);
SELECT check_jsonb_stats_test_estimate($$js @@ '$ IS ARRAY'$$, 0.1);

SELECT check_jsonb_stats_test_estimate($$js @@ 'bad_key = *'$$, 0.01);
SELECT check_jsonb_stats_test_estimate($$js @@ 'bad_key = null'$$, 0.01);
SELECT check_jsonb_stats_test_estimate($$js @@ '*.bad_key = *'$$, 0.01);
SELECT check_jsonb_stats_test_estimate($$js @@ '%.bad_key = *'$$, 0.01);
SELECT check_jsonb_stats_test_estimate($$js @@ 'bad_key.* = *'$$, 0.01);
SELECT check_jsonb_stats_test_estimate($$js @@ 'bad_key.# = *'$$, 0.01);
SELECT check_jsonb_stats_test_estimate($$js @@ 'bad_key.% = *'$$, 0.01);
SELECT check_jsonb_stats_test_estimate($$js @@ 'bad_key1.bad_key2 = *'$$, 0.01);

SELECT check_jsonb_stats_test_estimate($$js @@ 'foo = *'$$, 0.1);

SELECT check_jsonb_stats_test_estimate($$js @@ '#  IS OBJECT'$$, 0.1);
SELECT check_jsonb_stats_test_estimate($$js @@ '#: IS OBJECT'$$, 0.1);
SELECT check_jsonb_stats_test_estimate($$js @@ '#  IS ARRAY'$$, 0.1);
SELECT check_jsonb_stats_test_estimate($$js @@ '#: IS ARRAY'$$, 0.1);
SELECT check_jsonb_stats_test_estimate($$js @@ '#  IS STRING'$$, 0.1);
SELECT check_jsonb_stats_test_estimate($$js @@ '#: IS STRING'$$, 0.1);
SELECT check_jsonb_stats_test_estimate($$js @@ '#  IS NUMERIC'$$, 0.1);
SELECT check_jsonb_stats_test_estimate($$js @@ '#: IS NUMERIC'$$, 0.1);

SELECT check_jsonb_stats_test_estimate($$js @@ '# = *'$$, 0.1);
SELECT check_jsonb_stats_test_estimate($$js @@ '#: = *'$$, 0.1);
SELECT check_jsonb_stats_test_estimate($$js @@ '# = 3'$$, 0.1);
SELECT check_jsonb_stats_test_estimate($$js @@ '#: = 3'$$, 0.2);
SELECT check_jsonb_stats_test_estimate($$js @@ '# = 100'$$, 0.1);
SELECT check_jsonb_stats_test_estimate($$js @@ '#: = 100'$$, 0.1);

SELECT check_jsonb_stats_test_estimate($$js @@ '@# = 0'$$, 0.1);
SELECT check_jsonb_stats_test_estimate($$js @@ '@# = 1'$$, 0.1);
SELECT check_jsonb_stats_test_estimate($$js @@ '@# = 2'$$, 0.1);

SELECT check_jsonb_stats_test_estimate($$js @@ '# = "foo"'$$, 0.1);
SELECT check_jsonb_stats_test_estimate($$js @@ '#.# = "foo"'$$, 0.1);
SELECT check_jsonb_stats_test_estimate($$js @@ '#.% = "foo"'$$, 0.1);
SELECT check_jsonb_stats_test_estimate($$js @@ '#.* = "foo"'$$, 0.1);

SELECT check_jsonb_stats_test_estimate($$js @@ 'array = *'$$, 0.1);

SELECT check_jsonb_stats_test_estimate($$js @@ 'array.@# = 0'$$, 0.1);
SELECT check_jsonb_stats_test_estimate($$js @@ 'array.@# = 1'$$, 0.1);
SELECT check_jsonb_stats_test_estimate($$js @@ 'array.@# = 2'$$, 0.1);
SELECT check_jsonb_stats_test_estimate($$js @@ 'array.@# = 3'$$, 0.1);
SELECT check_jsonb_stats_test_estimate($$js @@ 'array.@# = 4'$$, 0.1);
SELECT check_jsonb_stats_test_estimate($$js @@ 'array.@# < 3'$$, 0.1);

SELECT check_jsonb_stats_test_estimate($$js @@ 'array @> [3]'$$, 0.1);
SELECT check_jsonb_stats_test_estimate2($$js @@ 'array @> [3,8]'$$, 1);

