<?php

namespace DDTrace\Integrations\Couchbase;

use DDTrace\Integrations\Integration;
use DDTrace\Obfuscation;
use DDTrace\SpanData;
use DDTrace\Type;

class CouchbaseIntegration extends Integration
{
    const NAME = 'couchbase';

    /**
     * @return string The integration name.
     */
    public function getName()
    {
        return self::NAME;
    }

    /**
     * Add instrumentation to Couchbase requests
     */
    public function init()
    {

        $this->traceCommand('remove');
        $this->traceCommand('insert');
        $this->traceCommand('upsert');
        $this->traceCommand('replace');
        $this->traceCommand('get');

        \DDTrace\trace_method('Couchbase\Bucket', 'query', function (SpanData $span, $args) {
            if (isset($args[0])) {
                $queryType = $this->getQueryType($args[0]);
                $this->addSpanDefaultMetadata($span, 'Couchbase\Bucket', 'query');
                $span->resource = $this->getQueryValue($args[0], $queryType);
            }
        });
    }

    public function traceCommand($command)
    {
        $integration = $this;
        \DDTrace\trace_method('Couchbase\Bucket', $command, function (SpanData $span, $args) use ($integration, $command) {
            $integration->addSpanDefaultMetadata($span, 'Couchbase\Bucket', $command);
            if (!is_array($args[0])) {
                $span->meta['Bucket.query'] = $command . ' ' . Obfuscation::toObfuscatedString($args[0]);
            }

            $integration->markForTraceAnalytics($span, $command);
        });
    }

    /**
     * Add basic span metadata shared but all spans generated by the Couchbase integration.
     *
     * @param SpanData $span
     * @param string $class
     * @param string $method
     */
    public function addSpanDefaultMetadata(SpanData $span, $class, $method)
    {
        $span->name = "$class.$method";
        $span->service = CouchbaseIntegration::NAME;
        $span->resource = $method;
        $span->type = Type::COUCHBASE;
    }

    /**
     * @param SpanData $span
     * @param string $command
     */
    public function markForTraceAnalytics(SpanData $span, $command)
    {
        $commandsForAnalytics = [
            'insert',
            'upsert',
            'replace',
            'get',
        ];

        if (in_array($command, $commandsForAnalytics)) {
            $this->addTraceAnalyticsIfEnabled($span);
        }
    }

    /**
     * Get query value.
     *
     * @param ViewQuery|N1qlQuery|SpatialViewQuery|SearchQuery  $query
     * @param string $type
     * @return string The query value.
     */
    public function getQueryValue($query, $type)
    {
        switch ($type) {
            case 'n1ql':
                return json_encode($query->options);
                break;
            case 'view':
            case 'spatialview':
                return json_encode($query->encode());
                break;
            case 'search':
                return json_encode($query);
                break;
            default:
                return get_class($query);
                break;
        }
    }

    /**
     * Get query type
     *
     * @param ViewQuery|N1qlQuery|SpatialViewQuery|SearchQuery $query
     *
     * @return string
     */
    public function getQueryType($query)
    {
        if ($query instanceof \Couchbase\N1qlQuery) {
            return 'n1ql';
        }
        if ($query instanceof \Couchbase\ViewQuery) {
            return 'view';
        }
        if ($query instanceof \Couchbase\searchQuery) {
            return 'search';
        }
        if ($query instanceof \Couchbase\SpatialViewQuery) {
            return 'spatialview';
        }
    }
}