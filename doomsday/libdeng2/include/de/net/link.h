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
#include <de/Observers>

namespace de
{   
    class Address;
    class Packet;
    class Socket;
    
    /**
     * Network communications link.
     *
     * @ingroup net
     */
    class PUBLIC_API Link
    {
    public:
        /// The remote end has closed the link. @ingroup errors
        DEFINE_ERROR(DisconnectedError);
        
        /// A packet of specific type was expected but something else was received instead. @ingroup errors
        DEFINE_ERROR(UnexpectedError);
        
        /// Specified timeout elapsed. @ingroup errors
        DEFINE_ERROR(TimeOutError);
        
        typedef SenderThread::OutgoingBuffer OutgoingBuffer;
        typedef ReceiverThread::IncomingBuffer IncomingBuffer;
        
        /// Observer interface for deleting the link.
        class IObserver {
        public:
            virtual ~IObserver() {}
            
            /**
             * Called when the observed Link is about to be deleted.
             *
             * @param link  Link begin deleted.
             */
            virtual void linkBeingDeleted(Link& link) = 0;
        };
        
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
         * Always sent on channel zero.
         *
         * @param data  Data to send.
         */
        Link& operator << (const IByteArray& data);
        
        /**
         * Sends a packet. The packet is first serialized and then sent.
         * Always sent on channel zero.
         *
         * @param packet  Packet.
         */
        Link& operator << (const Packet& packet);

        /**
         * Sends an array of data.
         *
         * @param data  Data to send.
         * @param channel  Channel to send on.
         *
         * @see MultiplexLink
         */
        void send(const IByteArray& data, duint channel = 0);

        /**
         * Sends a packet. The packet is first serialized and then sent.
         *
         * @param packet  Packet.
         * @param channel  Channel to send on.
         *
         * @see MultiplexLink
         */
        void send(const Packet& packet, duint channel = 0);
        
        /**
         * Receives a packet. Will not return until the packet has been received,
         * or the timeout has expired.
         *
         * @param timeOut  Maximum period of time to wait.
         *
         * @return  The received packet. Never returns NULL. Caller gets ownership
          *      of the packet.
         */
        Packet* receivePacket(const Time::Delta& timeOut = 10);

        /**
         * Receives a packet of specific type. Will not return until the packet has been received,
         * or the timeout has expired.
         *
         * @param timeOut  Maximum period of time to wait.
         *
         * @return  The received packet. Never returns NULL. Caller gets ownership
         *      of the packet.
         */
        template <typename Type>
        Type* receive(const Time::Delta& timeOut = 10) {
            Type* packet = dynamic_cast<Type*>(receivePacket(timeOut));
            if(!packet)
            {
                throw UnexpectedError("Link::receive", "Received wrong type of packet");
            }
            return packet;
        }
        
        /**
         * Receives an array of data. 
         *
         * The DisconnectedError is thrown if the remote end has closed the connection.
         *
         * @return  Received data array, or @c NULL if nothing has been received.
         *      Caller gets ownership of the returned object.
         */
        virtual Consignment* receive();
        
        /**
         * Checks if any incoming data has been received.
         */
        virtual bool hasIncoming() const;
        
        /**
         * Wait until all data has been sent.
         */
        void flush();
        
        /**
         * Returns the address of the remote end of the link.
         */ 
        Address peerAddress() const;
        
    protected:
        void initialize();

    public:
        typedef Observers<IObserver> Observers;
        Observers observers;
    
    private:
        /// Socket over which the link communicates.
        Socket* socket_; 

        /// Address of the remote end.
        Address peerAddress_;
        
        /// Thread that writes outgoing data to the socket.
        SenderThread* sender_;
        
        /// Thread that reads incoming data from the socket.
        ReceiverThread* receiver_;
        
        OutgoingBuffer outgoing_;
        IncomingBuffer incoming_;
    };
}

#endif /* LIBDENG2_LINK_H */
