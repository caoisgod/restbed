/*
 * Copyright (c) 2013, 2014, 2015 Corvusoft
 */

//System Includes
#include <thread>
#include <string>
#include <memory>
#include <ciso646>
#include <utility>
#include <stdexcept>
#include <functional>

//Project Includes
#include <restbed>
#include "http.hpp"

//External Includes
#pragma warning( disable 4702 )
#include <catch.hpp>
#pragma warning( restore 4702 )

//System Namespaces
using std::thread;
using std::string;
using std::function;
using std::exception;
using std::make_pair;
using std::shared_ptr;
using std::make_shared;

//Project Namespaces
using namespace restbed;

//External Namespaces

void error_handler( const int, const exception&, const shared_ptr< Session > session )
{
    session->close( 0, "I see nothing!", { { "Content-Length", "14" } } );
}

void faulty_method_handler( const shared_ptr< Session > )
{
    throw SERVICE_UNAVAILABLE;
}

SCENARIO( "custom service error handler", "[service]" )
{
    auto resource = make_shared< Resource >( );
    resource->set_path( "/resources/1" );
    resource->set_method_handler( "GET", faulty_method_handler );
    
    auto settings = make_shared< Settings >( );
    settings->set_port( 1984 );
    settings->set_default_header( "Connection", "close" );
    
    shared_ptr< thread > worker = nullptr;
    
    Service service;
    service.publish( resource );
    service.set_error_handler( error_handler );
    service.set_ready_handler( [ &worker ]( Service & service )
    {
        worker = make_shared< thread >( [ &service ] ( )
        {
            GIVEN( "I publish a faulty resources and set a service error handler" )
            {
                WHEN( "I perform a HTTP 'GET' request to '/resources/1'" )
                {
                    Http::Request request;
                    request.port = 1984;
                    request.host = "localhost";
                    request.path = "/resources/1";
                    
                    auto response = Http::get( request );
                    
                    THEN( "I should see a '0' (Custom Error) status code" )
                    {
                        REQUIRE( 0 == response.status_code );
                    }
                    
                    AND_THEN( "I should see a repsonse body of 'I see nothing!'" )
                    {
                        Bytes expection { 'I', ' ', 's', 'e', 'e', ' ', 'n', 'o', 't', 'h', 'i', 'n', 'g', '!' };
                        REQUIRE( response.body == expection );
                    }
                    
                    AND_THEN( "I should see a 'Connection' header value of 'close'" )
                    {
                        auto header = response.headers.find( "Connection" );
                        REQUIRE( header not_eq response.headers.end( ) );
                        REQUIRE( "close" == response.headers.find( "Connection" )->second );
                    }
                    
                    AND_THEN( "I should see a 'Content-Length' header value of '14'" )
                    {
                        auto header = response.headers.find( "Content-Length" );
                        REQUIRE( header not_eq response.headers.end( ) );
                        REQUIRE( "14" == response.headers.find( "Content-Length" )->second );
                    }
                }
                
                service.stop( );
            }
        } );
    } );
    
    service.start( settings );
    worker->join( );
}
