/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBDENG2_LINK_H
#define LIBDENG2_LINK_H

#include <de/SenderThread>
#include <de/ReceiverThread>

namespace de
{   
    class Address;
    class Socket;
    
    /**
     * Network communications link.
     *
     * @ingroup net
     */
    class PUBLIC_API Link
    {
    public:
        /**
         * Constructs a new communications link. A new socket is created for the link.
         *
         * @param address  Address to connect to.
         */
        Link(const Address& address);
        
        /**
         * Constructs a new communications link.
         *
         * @param socket  Socket for the link. Link gets ownership.
         */
        Link(Socket* socket);
        
        virtual ~Link();
        
        /**
         * Sends an array of data.
         *
         * @param data  Data to send.
         */
        void send(const IByteArray& data);
        
        /**
         * Receives an array of data.
         *
         * @return  Received data array, or @c NULL if nothing has been received.
         *      Caller gets ownership of the returned object.
         */
        IByteArray* receive();
        
        /**
         * Checks if any incoming data has been received.
         */
        bool hasIncoming() const {
            return !incoming_.empty();
        }

    protected:
        void initialize();
    
    private:
        /// Socket over which the link communicates.
        Socket* socket_; 
        
        /// Thread that writes outgoing data to the socket.
        SenderThread* sender_;
        
        /// Thread that reads incoming data from the socket.
        ReceiverThread* receiver_;
        
        SenderThread::OutgoingBuffer outgoing_;
        ReceiverThread::IncomingBuffer incoming_;
    };
}

#endif /* LIBDENG2_LINK_H */
