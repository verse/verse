/*globals ArrayBuffer, define*/

define(function() {
    var negotiation, string_message, int_message;

    /**
     * Abstract string message array buffer
     * @param message_type int
     * @param data_str string
     * @param feature_type int
     **/
    string_message = function(message_type, data_str, feature_type) {
        var buf, view, mes_len, i;

        mes_len = 3 + data_str.length;
        buf = new ArrayBuffer(mes_len);
        view = new DataView(buf);
        /* first byte - message type */
        view.setUint8(0, message_type);
        /* second byte - message length */
        view.setUint8(1, mes_len);
        /* third byte - feature type */
        view.setUint8(2, feature_type);
        /* Pack the data_str */
        for (i = 0; i < data_str.length; i++) {
            view.setUint8(3 + i, data_str.charCodeAt(i));
        }

        return buf;
    };

    /**
     * Abstract int message array buffer
     * @param message_type int
     * @param data_int int
     * @param feature_type int
     **/

    int_message = function(message_type, data_int, feature_type) {
        var buf, view;

        buf = new ArrayBuffer(4);
        view = new DataView(buf);
        /* first byte - message type */
        view.setUint8(0, message_type);
        /* second byte - message length */
        view.setUint8(1, 4);
        /* third byte - feature type */
        view.setUint8(2, feature_type);
        /* fourth byte - id */
        view.setUint8(3, data_int);

        return buf;
    };

    negotiation = {

        /* message types */
        CHANGE_L: 3,
        CHANGE_R: 4,
        CONFIRM_L: 5,
        CONFIRM_R: 6,

        /*
         * Flow Control ID (FCID)
         * feature type 1
         * @param type : int message type
         * @param id : fcid Value range: 0 - 255
         */
        fcid: function(type, id) {
            return int_message(type, id, 1);
        },

        /*
         * Congestion Control ID (CCID)
         * feature type 2
         * @param type : int message type
         * @param id : fcid Value range: 0 - 255
         */
        ccid: function(type, id) {
            return int_message(type, id, 2);
        },

        /*
         * URL of host defined in RFC 1738
         * feature type 3
         * @param type : int message type
         * @param nurl : string
         */
        url: function(type, nurl) {
            return string_message(type, nurl, 3);
        },

        /*
         * Cookie
         * feature type 4
         * @param type : int message type
         * @param cookie_val : string
         */
        cookie: function(type, cookie_val) {
            return string_message(type, cookie_val, 4);
        },


        /*
         * Data Exchange Definition (DED)
         * feature type 5
         * @param type : int message type
         * @param ded_val : string
         */
        ded: function(type, ded_val) {
            return string_message(type, ded_val, 5);
        },

        /*
         * Scale factor of RWIN used in Flow Control
         * feature type 6
         * @param type : int message type
         * @param id : rwin Value range: 0 - 255
         */
        rwin: function(type, value) {
            return int_message(type, value, 6);
        },

        /*
         * Frames per Seconds
         * feature type 7
         * @param type : int message type
         * @param fps: float Value range: Float min - Float max
         */

        fps: function(message_type, value) {
            var buf, view;

            buf = new ArrayBuffer(7);
            view = new DataView(buf);
            /* first byte - message type */
            view.setUint8(0, message_type);
            /* second byte - message length */
            view.setUint8(1, 7);
            /* third byte - feature type */
            view.setUint8(2, 7);
            /* fourth byte - value */
            view.setFloat32(3, value);

            return buf;
        },

        /*
         * Command Compression
         * feature type 8
         * @param type : int message type
         * @param id : compress Value range: 0 - 255
         */
        compression: function(type, value) {
            return int_message(type, value, 8);
        }



    };

    return negotiation;

});
