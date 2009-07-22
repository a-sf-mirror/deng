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

#include <de/Transceiver>
#include <de/SenderThread>
#include <de/ReceiverThread>
#include <de/Observers>
#include <de/Flag>

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
    class PUBLIC_API Link : public Transceiver
    {
    public:
        /// The remote end has closed the link. @ingroup errors
        DEFINE_ERROR(DisconnectedError);
        
        typedef SenderThread::OutgoingBuffer OutgoingBuffer;
        typedef ReceiverThread::IncomingBuffer IncomingBuffer;
        
        /// Link observer interface.
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
        
        /// Sending on channel 1 instead of the default 0.
        DEFINE_FINAL_FLAG(CHANNEL_1, 0, Mode);
        
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

        // Implements Transceiver.
        void send(const IByteArray& data);
        Message* receive();
      
        /**
         * Checks if any incoming data has been received.
         */
        bool hasIncoming() const;
        
        /**
         * Wait until all data has been sent.
         */
        void flush();

        /**
         * Returns the socket over which the Link communicates.
         *
         * @return  Socket.
         */ 
        Socket& socket() { return *socket_; }
        
        /**
         * Returns the address of the remote end of the link.
         */ 
        Address peerAddress() const;
        
    protected:
        void initialize();

    public:
        typedef Observers<IObserver> Observers;
        Observers observers;

        /// Mode flags.
        Mode mode;
    
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
