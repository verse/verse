"use strict";

/*globals define, ArrayBuffer*/
define(["response"], function(response) {

    describe("Response", function() {
        var mock_buff, view;


        describe("parse", function() {
            beforeEach(function() {
                mock_buff = new ArrayBuffer(8);
                view = new DataView(mock_buff);
                /*message lenght*/
                view.setUint16(2, 8);
                /* command usr_auth_fail */
                view.setUint8(4, 8);
                /* command length*/
                view.setUint8(5, 3);
                /* command method*/
                view.setUint8(6, 2);
            });

            it("should return passwd for password mock message", function() {
                expect(response.parse(mock_buff)).toEqual("passwd");
            });


        });




    });
});
