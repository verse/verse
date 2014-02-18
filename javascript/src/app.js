/* jshint devel: true, unused: true */
/* global require */

require(['wsocket'], function(wsocket) {
    'use strict';

    var config;

    config = {
        uri: 'ws://verse.tul.cz:23456',
        version: 'v1.verse.tul.cz',
        username: 'albert',
        passwd: 'aaa'
    };

    wsocket.init(config);


});
