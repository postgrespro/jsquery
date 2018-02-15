CREATE FUNCTION json_jsquery_filter(jsonb, jsquery)
	RETURNS jsonb
	AS 'MODULE_PATHNAME'
	LANGUAGE C STRICT IMMUTABLE;

CREATE OPERATOR ~~ (
	LEFTARG = jsonb,
	RIGHTARG = jsquery,
	PROCEDURE = json_jsquery_filter
);

