-- complain if script is sourced in psql, rather than via ALTER EXTENSION
\echo Use "ALTER EXTENSION jsquery UPDATE TO '1.1'" to load this file. \quit

CREATE OR REPLACE FUNCTION parse_mquery(cstring)
	RETURNS jsquery
	AS 'MODULE_PATHNAME'
	LANGUAGE C STRICT IMMUTABLE;