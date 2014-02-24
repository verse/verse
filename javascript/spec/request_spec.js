"use strict";

/*globals define*/

define(["request"], function(request) {

    describe("Request", function() {
        var shake, view, auth, uname, passwd;


        describe("handshake", function() {
            beforeEach(function() {
                uname = "albert";
                shake = request.handshake(uname);
                view = new DataView(shake);
            });

            it("should have handshake.lenght equal to 14 for name=albert ", function() {
                expect(shake.byteLength).toEqual(14);
            });

            it("first byte - message lenght - should be 14 for albert", function() {
                expect(view.getUint16(2)).toEqual(14);
            });

            it("lenght of command should be 10 (4 + 6) for albert", function() {
                expect(view.getUint8(5)).toEqual(10);
            });

            it("Pack length of string should be 6 for albert", function() {
                expect(view.getUint8(6)).toEqual(6);
            });

            it("First char of packed string should a", function() {
                expect(view.getUint8(7)).toEqual("a".charCodeAt(0));
            });

        });

        describe("userAuth", function() {
            beforeEach(function() {
                uname = "albert";
                passwd = "12345";
                auth = request.userAuth(uname, passwd);
                view = new DataView(auth);
            });

            it("should have handshake.lenght equal to 20", function() {
                expect(auth.byteLength).toEqual(20);
            });

            it("The lenght of the message should be 20", function() {
                expect(view.getUint16(2)).toEqual(20);
            });

            it("Pack length of the command  should be 16", function() {
                expect(view.getUint8(5)).toEqual(5 + uname.length + passwd.length);
            });

            it("Pack length of the username should be 6", function() {
                expect(view.getUint8(6)).toEqual(uname.length);
            });

            it("First char of packed username should be a", function() {
                expect(view.getUint8(7)).toEqual("a".charCodeAt(0));
            });

            it("Pack length of the passwd should be 5", function() {
                expect(view.getUint8(14)).toEqual(passwd.length);
            });

            it("First char of packed passwd should be 1", function() {
                expect(view.getUint8(15)).toEqual("1".charCodeAt(0));
            });

            it("Last char of packed passwd should be 5", function() {
                expect(view.getUint8(15 + passwd.length - 1)).toEqual("5".charCodeAt(0));
            });



        });

    });


});
