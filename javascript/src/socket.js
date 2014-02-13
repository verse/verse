/* jshint browser: true*/
/* globals request, response*/
/*exported wsocket*/

var wsocket = (function(request, response) {
    'use strict';
    window.WebSocket = window.WebSocket || window.MozWebSocket;
    var my_webscoket;

    window.onbeforeunload = function() {
        my_webscoket.onclose = function() {}; // disable onclose handler first
        my_webscoket.close();
    };

    var wsocket = {
        init: function(config) {
            console.log('Connecting to URI:' + config.uri + ' ...');
            my_webscoket = new WebSocket(config.uri, config.version);
            my_webscoket.binaryType = 'arraybuffer';

            my_webscoket.addEventListener('open', function(e) {
                wsocket.onConnect(e, config.username);
            });
            my_webscoket.addEventListener('error', wsocket.onError);
            my_webscoket.addEventListener('close', wsocket.onClose);
            my_webscoket.addEventListener('message', function(e) {
                wsocket.onMessage(e, config.username, config.passwd);
            });

        },



        onConnect: function onConnect(event, username) {
            var buf;
            console.log('[Connected] ' + event.code);
            buf = request.handshake(username);
            my_webscoket.send(buf);
            console.log('handshake send');
        },

        onError: function onError(event) {
            console.log('Error:' + event.data);
        },

        onClose: function onClose(event) {
            console.log('[Disconnected], Code:' + event.code + ', Reason: ' + event.reason);
        },

        onMessage: function onMessage(message, username, passwd) {
            var buf, response_type;
            if (message.data instanceof ArrayBuffer) {
                if (!response.checkHeader(message.data)) {
                    /*bad protocl version - close conection*/
                    my_webscoket.close();
                    return;
                }

                response_type = response.parse(message.data);
                if (response_type === 'passwd') {
                    buf = request.userAuth(username, passwd);
                    /* Send the blob */
                    my_webscoket.send(buf);
                    console.log('heslo zaslano');
                }

            }
        }


    };

    return wsocket;

}(request, response));
