/*globals ArrayBuffer*/
/*exported request*/
var request = (function() {
    'use strict';


    var request = {


        handshake: function(name) {
            var i;
            /* Fill buffer with data of Verse header and user_auth
             * command */
            var message_len = name.length + 8;
            var buf = new ArrayBuffer(message_len);
            var view = new DataView(buf);

            /* Verse header starts with version */
            /* First 4 bits are reserved for version of protocol */
            view.setUint8(0, 1 << 4);
            /* The lenght of the message */
            view.setUint16(2, message_len);
            /* Pack OpCode of user_auth command */
            view.setUint8(4, 7);
            /* Pack length of the command */
            view.setUint8(5, 4 + name.length);
            /* Pack length of string */
            view.setUint8(6, name.length);
            /* Pack the string of the username */
            for (i = 0; i < name.length; i++) {
                view.setUint8(7 + i, name.charCodeAt(i));
            }
            /* Pack method NONE ... it is not supported and server will
             * return list of supported methods in command user_auth_fail */
            view.setUint8(7 + name.length, 1);

            return buf;
        },

        userAuth: function(name, passwd) {


            var i;

            /* Fill buffer with data of Verse header and user_auth
             * command */
            var message_len = 9 + name.length + passwd.length;
            var buf = new ArrayBuffer(message_len);
            var view = new DataView(buf);
            /* Verse header starts with version */
            /* First 4 bits are reserved for version of protocol */
            view.setUint8(0, 1 << 4);
            /* The lenght of the message */
            view.setUint16(2, message_len);
            /* Pack OpCode of user_auth command */
            view.setUint8(4, 7);
            /* Pack length of the command */
            view.setUint8(5, 5 + name.length + passwd.length);
            /* Pack length of string */
            view.setUint8(6, name.length);
            /* Pack the string of the username */
            for (i = 0; i < name.length; i++) {
                view.setUint8(7 + i, name.charCodeAt(i));
            }
            /* Pack method Password  */
            view.setUint8(7 + name.length, 2);
            /* Pack password length */
            view.setUint8(8 + name.length, passwd.length);
            /* Pack the string of the password */
            for (i = 0; i < passwd.length; i++) {
                view.setUint8(9 + name.length + i, passwd.charCodeAt(i));
            }

            /* Send the blob */
            return buf;
        }
    };

    return request;

}());
