var request = (function() {
    'use strict';


    var request = {
        handshake: function(name) {
            var i;
            /* Fill buffer with data of Verse header and user_auth
             * command */
            var buf_pos = 0;
            var name_len = name.length;
            var message_len = name_len + 8;

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
            view.setUint8(buf_pos, 4 + name_len);
            buf_pos += 1;
            /* Pack length of string */
            view.setUint8(buf_pos, name_len);
            buf_pos += 1;
            /* Pack the string of the username */
            for (i = 0; i < name_len; i++) {
                view.setUint8(buf_pos + i, name.charCodeAt(i));
            }
            buf_pos += name_len;
            /* Pack method NONE ... it is not supported and server will
             * return list of supported methods in command user_auth_fail */
            view.setUint8(buf_pos, 1);
            buf_pos += 1;

            return buf;
        },

        userAuth: function(name, passwd) {

            var name_len = name.length;
            var pass_len = passwd.length;

            var buf_pos = 0;
            var i;

            /* Fill buffer with data of Verse header and user_auth
             * command */
            var message_len = 4 + 1 + 1 + 1 + name_len + 1 + 1 + pass_len;
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
            view.setUint8(buf_pos, 1 + 1 + 1 + name_len + 1 + 1 + pass_len);
            buf_pos += 1;
            /* Pack length of string */
            view.setUint8(buf_pos, name_len);
            buf_pos += 1;
            /* Pack the string of the username */
            for (i = 0; i < name_len; i++) {
                view.setUint8(buf_pos + i, name.charCodeAt(i));
            }
            buf_pos += name_len;

            /* Pack method Password  */
            view.setUint8(buf_pos, 2);
            buf_pos += 1;
            /* Pack password length */
            view.setUint8(buf_pos, pass_len);
            buf_pos += 1;
            /* Pack the string of the password */
            for (i = 0; i < pass_len; i++) {
                view.setUint8(buf_pos + i, passwd.charCodeAt(i));
            }
            buf_pos += pass_len;

            /* Send the blob */
            return buf;
        }
    };

    return request;

}());
