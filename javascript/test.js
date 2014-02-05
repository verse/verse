    (function(messages) {
        'use strict';

        var test1, test2;

        function assertEqual(buf1, buf2) {
            if (buf1.byteLength != buf2.byteLength) return 'lenght is not the same';
            var dv1 = new Uint8Array(buf1);
            var dv2 = new Uint8Array(buf2);
            for (var i = 0; i != buf1.byteLength; i++) {
                if (dv1[i] != dv2[i]) return 'difference on byte i ' + i;
            }
            return true;
        }

        var testComposeMessage = function(name) {
            /* Fill buffer with data of Verse header and user_auth
             * command */
            var buf_pos = 0;
            var name_len = name.length;
            var message_len = 4 + 1 + 1 + 1 + name_len + 1;

            var buf = new ArrayBuffer(message_len);
            var view = new DataView(buf);
            /* Verse header starts with version */
            view.setUint8(buf_pos, 1 << 4); /* First 4 bits are reserved for version of protocol */
            buf_pos += 2;
            /* The lenght of the message */
            view.setUint16(buf_pos, message_len);
            buf_pos += 2;

            /* Pack OpCode of user_auth command */
            view.setUint8(buf_pos, 7);
            buf_pos += 1;
            /* Pack length of the command */
            view.setUint8(buf_pos, 1 + 1 + 1 + name_len + 1);
            buf_pos += 1;
            /* Pack length of string */
            view.setUint8(buf_pos, name_len);
            buf_pos += 1;
            /* Pack the string of the username */
            for (var i = 0; i < name_len; i++) {
                view.setUint8(buf_pos + i, name.charCodeAt(i));
            }
            buf_pos += name_len;
            /* Pack method NONE ... it is not supported and server will
             * return list of supported methods in command user_auth_fail */
            view.setUint8(buf_pos, 1);
            buf_pos += 1;

            return buf;
        };

        test1 = testComposeMessage('alan');
        test2 = messages.composeHandshake('alan');

        console.log(assertEqual(test1, test2));


    }(messages));
