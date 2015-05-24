/*
 * Copyright (c) 2013, 2014, 2015 Corvusoft
 */

//System Includes
#include <stdexcept>

//Project Includes
#include "corvusoft/restbed/methods.h"
#include "corvusoft/restbed/session.h"
#include "corvusoft/restbed/detail/resource_impl.h"

//External Includes
#include <corvusoft/framework/string>
#include <corvusoft/framework/unique_id>

//System Namespaces
using std::set;
using std::pair;
using std::string;
using std::function;
using std::multimap;
using std::shared_ptr;
using std::invalid_argument;

//Project Namespaces

//External Namespaces
using framework::String;
using framework::UniqueId;

namespace restbed
{
    namespace detail
    {
        ResourceImpl::ResourceImpl( void ) : m_paths( ),
            m_authentication_handler( nullptr ),
            m_method_handlers( )
        {
            return;
        }
        
//        ResourceImpl::ResourceImpl( const ResourceImpl& original ) : m_id( original.m_id ),
//            m_paths( original.m_paths ),
//            m_method_handlers( original.m_method_handlers )
//        {
//            return;
//        }

        ResourceImpl::~ResourceImpl( void )
        {
            return;
        }

        void ResourceImpl::authenticate( const shared_ptr< Session >& session,
                                         const function< void ( const shared_ptr< Session >& ) >& callback )
        {
            if ( m_authentication_handler not_eq nullptr )
            {
                m_authentication_handler( session );
            }
            else
            {
                callback( session );
            }
        }

        const set< string >& ResourceImpl::get_paths( void ) const
        {
            return m_paths;
        }
        
        multimap< string, pair< multimap< string, string >, function< void ( const shared_ptr< Session >& ) > > >
        ResourceImpl::get_method_handlers( const string& method ) const
        {
            if ( method.empty( ) )
            {
                return m_method_handlers;
            }

            return decltype( m_method_handlers )( m_method_handlers.lower_bound( method ),
                                                  m_method_handlers.upper_bound( method ) );
        }

        void ResourceImpl::set_paths( const set< string >& values )
        {
            m_paths = values;
        }
        
        void ResourceImpl::set_method_handler( const string& method,
                                               const multimap< string, string >& filters,
                                               const std::function< void ( const std::shared_ptr< Session >& ) >& callback )
        {
            const string verb = String::uppercase( method );

            if ( methods.count( verb ) == 0 )
            {
                throw invalid_argument(
                    String::format( "Resource method handler set with an unsupported HTTP method '%s'.", verb.data( ) )
                );
            }
            
            m_method_handlers.insert( make_pair( verb, make_pair( filters, callback ) ) );
        }

        void ResourceImpl::set_authentication_handler( const function< void ( const shared_ptr< Session >& ) >& value )
        {
            m_authentication_handler = value;
        }

        void ResourceImpl::set_error_handler( const function< void ( const int, const shared_ptr< Session >& ) >& value )
        {
            //m_error_handler = value;
        }
    }
}
