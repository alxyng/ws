# WS

## TODO:

- random disconnect sometimes when client sends message in chat example?
- Licensing...
- Allow specifying max websocket message length to receive
- Opening/Closing handshake timeouts
- Test support for 64bit payload lengths
- WSS example

## A note about `session_base` in the examples

In the examples, a class called `session_base` is used to fully initialise the `socket_` member before passing a reference to that member to `ws::session`. This is an example of the C++ [base-from-member idiom](https://en.wikibooks.org/wiki/More_C%2B%2B_Idioms/Base-from-Member). Even though the `ws::session` constructor only stores a reference to the `socket_` member and doesn't call any of it's member functions ([which is defined behaviour](http://stackoverflow.com/questions/34477383/passing-a-reference-to-an-uninitialised-object-to-a-super-class-constructor-and/34492547#34492547)), it may do in the future which could introduce bugs.

Using `boost::base_from_member<tcp::socket>` caused a compiler error which I think is because of the move-constructor needed to initialise the `tcp::socket` member.
