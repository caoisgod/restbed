/*
 * Copyright (c) 2013, 2014, 2015 Corvusoft
 */

//System Includes
#include <regex>
#include <cstdio>
#include <stdexcept>
#include <functional>

//Project Includes
#include "corvusoft/restbed/logger.h"
#include "corvusoft/restbed/request.h"
#include "corvusoft/restbed/session.h"
#include "corvusoft/restbed/resource.h"
#include "corvusoft/restbed/settings.h"
#include "corvusoft/restbed/status_message.h"
#include "corvusoft/restbed/session_manager.h"
#include "corvusoft/restbed/detail/service_impl.h"
#include "corvusoft/restbed/detail/session_impl.h"
#include "corvusoft/restbed/detail/resource_impl.h"
#include "corvusoft/restbed/detail/session_manager_impl.h"

//External Includes
#include <corvusoft/framework/string>

//System Namespaces
using std::set;
using std::find;
using std::bind;
using std::regex;
using std::string;
using std::find_if;
using std::function;
using std::to_string;
using std::exception;
using std::shared_ptr;
using std::make_shared;
using std::runtime_error;
using std::shared_ptr;
using std::invalid_argument;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

//Project Namespaces

//External Namespaces
using asio::ip::tcp;
using asio::io_service;
using asio::error_code;
using asio::socket_base;
using asio::system_error;
using framework::String;

namespace restbed
{
    namespace detail
    {
        ServiceImpl::ServiceImpl( const Settings& settings ) : m_port( settings.get_port( ) ),
            m_root( settings.get_root( ) ),
            // m_maximum_connections( settings.get_maximum_connections( ) ),
            // m_connection_timeout( settings.get_connection_timeout( ).count( ) ),
            m_resource_routes( ),
            m_log_handler( nullptr ),
            m_io_service( nullptr ),
            m_session_manager( nullptr ),
            m_acceptor( nullptr ),
            m_authentication_handler( nullptr ),
            m_error_handler( nullptr )
        {
            return;
        }
        
        ServiceImpl::~ServiceImpl( void )
        {
            try
            {
                stop( );
            }
            catch ( ... )
            {
                log( Logger::Level::WARNING, "Service failed graceful teardown." );
            }
        }
        
        void ServiceImpl::stop( void )
        {
            if ( m_io_service not_eq nullptr )
            {
                m_io_service->stop( );
            }
        }

        void ServiceImpl::start( void )
        {
            m_io_service = make_shared< io_service >( );

            Settings settings; //get from constructor + logger...
            m_session_manager = make_shared< SessionManagerImpl >( settings );

            m_acceptor = make_shared< tcp::acceptor >( *m_io_service, tcp::endpoint( tcp::v6( ), m_port ) );
            m_acceptor->set_option( socket_base::reuse_address( true ) );
            m_acceptor->listen(  );//m_maximum_connections );
            
            listen( );

            // log( Logger::Level::INFO, "Service online at 'TIME HERE PLEASE'" );

            m_io_service->run( );

            // log( Logger::Level::INFO, "Service halted at 'TIME HERE PLEASE'" );
        }
        
        void ServiceImpl::publish( const shared_ptr< Resource >& resource )
        {
            if ( resource == nullptr )
            {
                return;
            }

            auto paths = resource->get_paths( );

            if ( not has_unique_paths( paths ) ) //paths_case_insensitive!!!!!!
            {
                throw invalid_argument( "Resource would pollute namespace. Please ensure all published resources have unique paths." );
            }

            for ( auto& path : paths )
            {
                //if ( settings.paths_case_insensitive ) then String::lowercase( )

                //path = normalise_path( root, path );
                m_resource_routes[ path ] = resource;
            }

            //log( Logger::Level::INFO,
            //     String::format( "Published resource at '%s'", String::join( resource.get_paths( ), ", " ) ) );
        }
        
        void ServiceImpl::suppress( const shared_ptr< Resource >& resource )
        {
            if ( resource == nullptr )
            {
                return;
            }

            for ( const auto& path : resource->get_paths( ) )
            {
                if ( m_resource_routes.erase( path ) )
                {
//                    log( Logger::Level::INFO, String::format( "Suppressed resource route '%s'.", path.data( ) ) );
                }
                else
                {
//                    log( Logger::Level::WARNING, String::format( "Failed to suppress resource route '%s'; Not Found!", path.data( ) ) );
                }
            }
        }
        
        void ServiceImpl::set_log_handler(  const shared_ptr< Logger >& value )
        {
            //if is running throw runtime_error
            m_log_handler = value;
        }
        
        void ServiceImpl::set_authentication_handler( const function< void ( const shared_ptr< Session >&,
                                                                             const function< void ( const shared_ptr< Session >& ) >& ) >& value )
        {
            //if is running throw runtime_error
            m_authentication_handler = value;
        }
        
        void ServiceImpl::set_error_handler( const function< void ( const int, const shared_ptr< Session >& ) >& value )
        {
            //if is running throw runtime_error
            m_error_handler = value;
        }
        
        void ServiceImpl::listen( void )
        {
            auto socket = make_shared< tcp::socket >( m_acceptor->get_io_service( ) );
            
            m_acceptor->async_accept( *socket, bind( &ServiceImpl::create_session, this, socket, _1 ) );
        }

        void ServiceImpl::route( const shared_ptr< Session >& session )
        {
            if ( session->is_closed( ) )
            {
                return;
            }

            const auto request = session->get_request( );
            const auto resource = session->get_resource( );

            function< void ( const std::shared_ptr< Session >& ) > method_handler = nullptr;
            const auto method_handlers = resource->get_method_handlers( request->get_method( ) );

            for ( const auto& handler : method_handlers )
            {
                const auto& filters = handler.second.first;

                bool valid = true;
                for ( const auto& filter : filters )
                {
                    for ( const auto& header : request->get_headers( filter.first ) )
                    {
                         if ( not regex_match( header.second, regex( filter.second ) ) )
                         {
                             valid = false;
                             //method_handler = resource->get_failed_filter_validation_handler( );???
                             break;
                         }
                    }

                    if ( not valid ) break;
                }

                if ( valid )
                {
                    method_handler = handler.second.second;
                    break;
                }
            }

            if ( method_handler == nullptr )
            {
                //if ( m_service_methods.count( session.get_request( ).get_method( ) ) == 0 )
                //{
                //    session->close( 501, status_message.at( 501 ) );
                //    return;
                //return method_not_implemented_handler( session );
                //}
                //else
                //{
                return method_not_allowed( session );
                //}
            }

            method_handler( session );
        }

        void ServiceImpl::resource_router( const shared_ptr< Session >& session )
        {
            if ( session->is_closed( ) )
            {
                return;
            }

            const auto request = session->get_request( );
            const auto resource_route = m_resource_routes.find( request->get_path( ) );

            if ( resource_route == m_resource_routes.end( ) )
            {
                return not_found( session );
            }

            const auto resource = resource_route->second;
            session->m_pimpl->set_resource( resource );

            resource->m_pimpl->authenticate( session, bind( &ServiceImpl::route, this, _1 ) );
        }

        void ServiceImpl::create_session( const shared_ptr< tcp::socket >& socket, const error_code& error )
        {
            if ( not error )
            {
                const function< void ( const shared_ptr< Session >& ) > route = bind( &ServiceImpl::resource_router, this, _1 );
                const function< void ( const shared_ptr< Session >& ) > load = bind( &SessionManager::load, m_session_manager, _1, route );
                const function< void ( const shared_ptr< Session >& ) > authenticate = bind( &ServiceImpl::authenticate, this, _1, load );

                m_session_manager->create( [ socket, authenticate ]( const shared_ptr< Session >& session )
                {
                    session->m_pimpl->set_socket( socket );
                    session->m_pimpl->fetch( session, authenticate );
                    //session->m_pimpl->set_default_headers( m_default_headers );
                } );
            }
            else
            {
                //socket.close()?
                //log, error handler, close connection.
            }

            listen( );
        }

        void ServiceImpl::log( const Logger::Level level, const string& message )
        {
            // if ( m_log_handler not_eq nullptr )
            // {
            //     m_log_handler->log( level, "%s", message.data( ) );
            // }
        }

        void ServiceImpl::authenticate( const shared_ptr< Session >& session,
                                        const function< void ( const shared_ptr< Session >& ) >& callback )
        {
            if ( m_authentication_handler not_eq nullptr )
            {
                m_authentication_handler( session, callback );
            }
            else
            {
                callback( session );
            }
        }

        //error
        void ServiceImpl::error( const int status_code, const shared_ptr< Session >& session )
        {
//            if ( m_error_handler not_eq nullptr )
//            {
//                m_error_handler( status_code, callback );
//            }
//            else
//            {
//                callback( session );
//            }

            // const auto& iterator = status_codes.find( status_code );

            // const string status_message = ( iterator not_eq status_codes.end( ) ) ?
            //                                 iterator->second :
            //                                 "No Appropriate Status Message Found";
            
            // log( Logger::Level::ERROR, String::format( "Error %i (%s) requesting '%s' resource\n",
            //                                            status_code,
            //                                            status_message.data( ),
            //                                            request.get_path( ).data( ) ) );
                                                  
            // response.set_status_code( status_code );
            // response.set_header( "Content-Type", "text/plain; charset=us-ascii" );
            // response.set_body( status_message );
        }

        void ServiceImpl::not_found( const std::shared_ptr< Session >& session )
        {
            session->close( 404, status_message.at( 404 ) );
        }

        void ServiceImpl::method_not_allowed( const std::shared_ptr< Session >& session )
        {
            session->close( 405, status_message.at( 405 ) );
        }

        void ServiceImpl::method_not_implemented( const std::shared_ptr< Session >& session )
        {
            session->close( 501, status_message.at( 501 ) );
        }

        bool ServiceImpl::has_unique_paths( const set< string >& paths )
        {
            for ( const auto& path : paths )
            {
                if ( m_resource_routes.count( path ) )
                {
                    return false;
                }
            }

            return true;
        }

        //void ServiceImpl::set_socket_timeout( shared_ptr< tcp::socket > socket )
        //{
            // struct timeval value;
            // value.tv_usec = 0;
            // value.tv_sec = m_connection_timeout;

            // auto native_socket = socket->native( );
            // int status = setsockopt( native_socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast< char* >( &value ), sizeof( value ) );

            // if ( status == -1 )
            // {
            //     throw runtime_error( "Failed to set socket receive timeout" );
            // }

            // status = setsockopt( native_socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast< char* >( &value ), sizeof( value ) );

            // if ( status == -1 )
            // {
            //     throw runtime_error( "Failed to set socket send timeout" );
            // }
        //}
    }
}
