"use strict";

/*globals define*/

define(["negotiation"], function(negotiation) {

    describe("Negotiation", function() {
        var nego, view;


        describe("fcid", function() {
            beforeEach(function() {
                nego = negotiation.fcid(negotiation.CHANGE_R, 15);
                view = new DataView(nego);
            });

            it("lenght of fcid message should be equal to 4", function() {
                expect(nego.byteLength).toEqual(4);
            });

            it("first byte - message type - should be 4 for CHANGE_R", function() {
                expect(view.getUint8(0)).toEqual(4);
            });

            it("second byte - message lenght - should be 4 ", function() {
                expect(view.getUint8(1)).toEqual(4);
            });

            it("third byte - feature type - should be 1 ", function() {
                expect(view.getUint8(2)).toEqual(1);
            });

            it("fourth byte - id - should be 15 ", function() {
                expect(view.getUint8(3)).toEqual(15);
            });

            

        });

        describe("ccid", function() {
            beforeEach(function() {
                nego = negotiation.ccid(negotiation.CHANGE_L, 215);
                view = new DataView(nego);
            });

            it("lenght of ccid message should be equal to 4", function() {
                expect(nego.byteLength).toEqual(4);
            });

            it("first byte - message type - should be 3 for CHANGE_L", function() {
                expect(view.getUint8(0)).toEqual(3);
            });

            it("second byte - message lenght - should be 4 ", function() {
                expect(view.getUint8(1)).toEqual(4);
            });

            it("third byte - feature type - should be 2 ", function() {
                expect(view.getUint8(2)).toEqual(2);
            });

            it("fourth byte - id - should be 15 ", function() {
                expect(view.getUint8(3)).toEqual(215);
            });

            

        });

        describe("url", function() {
            var url;

            beforeEach(function() {
                url = 'wc://verse.example.com:12345';
                nego = negotiation.url(negotiation.CONFIRM_L, url);
                view = new DataView(nego);
            });

            it("lenght of ccid message should be equal to 3 + 28 (url lenght)", function() {
                expect(nego.byteLength).toEqual(3 + 28);
            });

            it("first byte - message type - should be 5 for CONFIRM_L", function() {
                expect(view.getUint8(0)).toEqual(5);
            });

            it("second byte - message lenght - should be 3 + 28 ", function() {
                expect(view.getUint8(1)).toEqual(31);
            });

            it("third byte - feature type - should be 3 ", function() {
                expect(view.getUint8(2)).toEqual(3);
            });

            it("First char of packed url should be w", function() {
                expect(view.getUint8(3)).toEqual("w".charCodeAt(0));
            });

            it("Last char of packed passwd should be 5", function() {
                expect(view.getUint8(30)).toEqual("5".charCodeAt(0));
            });

           
            

        });

        describe("cookie", function() {
            var cookie;

            beforeEach(function() {
                cookie = 'some cookie value';
                nego = negotiation.cookie(negotiation.CONFIRM_L, cookie);
                view = new DataView(nego);
            });

            it("lenght of ccid message should be equal to 3 + 17 (cookie lenght)", function() {
                expect(nego.byteLength).toEqual(20);
            });

            it("first byte - message type - should be 5 for CONFIRM_L", function() {
                expect(view.getUint8(0)).toEqual(5);
            });

            it("second byte - message lenght - should be 3 + 17 ", function() {
                expect(view.getUint8(1)).toEqual(20);
            });

            it("third byte - feature type - should be 4 ", function() {
                expect(view.getUint8(2)).toEqual(4);
            });

            it("First char of packed url should be s", function() {
                expect(view.getUint8(3)).toEqual("s".charCodeAt(0));
            });

            it("Last char of packed passwd should be e", function() {
                expect(view.getUint8(19)).toEqual("e".charCodeAt(0));
            });


        });

         describe("fps", function() {
            var fps_val;

            beforeEach(function() {
                fps_val = 7.244;
                nego = negotiation.fps(negotiation.CONFIRM_R, fps_val);
                view = new DataView(nego);
            });

            it("lenght of ccid message should be equal to 7", function() {
                expect(nego.byteLength).toEqual(7);
            });

            it("first byte - message type - should be 6 for CONFIRM_R", function() {
                expect(view.getUint8(0)).toEqual(6);
            });

            it("second byte - message lenght - should be 7 ", function() {
                expect(view.getUint8(1)).toEqual(7);
            });

            it("third byte - feature type - should be 7 ", function() {
                expect(view.getUint8(2)).toEqual(7);
            });

            it("fourth byte - FPS - should be 7.244 ", function() {
                expect(view.getFloat32(3)).toBeCloseTo(7.244);
            });

           


        });
        

    });


});
