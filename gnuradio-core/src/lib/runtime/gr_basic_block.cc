/* -*- c++ -*- */
/*
 * Copyright 2006 Free Software Foundation, Inc.
 *
 * This file is part of GNU Radio
 *
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gr_basic_block.h>
#include <gr_block_registry.h>
#include <stdexcept>
#include <sstream>

using namespace pmt;

static long s_next_id = 0;
static long s_ncurrently_allocated = 0;

long
gr_basic_block_ncurrently_allocated()
{
    return s_ncurrently_allocated;
}

gr_basic_block::gr_basic_block(const std::string &name,
                               gr_io_signature_sptr input_signature,
                               gr_io_signature_sptr output_signature)
  : d_name(name),
    d_input_signature(input_signature),
    d_output_signature(output_signature),
    d_unique_id(s_next_id++),
    d_symbolic_id(global_block_registry.block_register(this)),
    d_symbol_name(global_block_registry.register_symbolic_name(this)),
    d_color(WHITE),
    message_subscribers(pmt::pmt_make_dict())
{
    s_ncurrently_allocated++;
}

gr_basic_block::~gr_basic_block()
{
    s_ncurrently_allocated--;
    global_block_registry.block_unregister(this);
}

gr_basic_block_sptr
gr_basic_block::to_basic_block()
{
    return shared_from_this();
}

void
gr_basic_block::set_block_alias(std::string name)
{ 
    global_block_registry.register_symbolic_name(this, name); 
}

// ** Message passing interface **

//  - register a new input message port
void gr_basic_block::message_port_register_in(pmt::pmt_t port_id){
    msg_queue[port_id] = msg_queue_t();
    }

//  - register a new output message port
void gr_basic_block::message_port_register_out(pmt::pmt_t port_id){
    if(!pmt::pmt_is_symbol(port_id)){ throw std::runtime_error("bad port id"); }
    if(pmt::pmt_dict_has_key(message_subscribers, port_id)){ throw std::runtime_error("port already in use"); }
    message_subscribers = pmt::pmt_dict_add(message_subscribers, port_id, pmt::PMT_NIL);
    }

//  - publish a message on a message port
void gr_basic_block::message_port_pub(pmt::pmt_t port_id, pmt::pmt_t msg){
    if(!pmt::pmt_dict_has_key(message_subscribers, port_id)){ throw std::runtime_error("port does not exist"); }
    pmt::pmt_t currlist = pmt::pmt_dict_ref(message_subscribers,port_id,pmt::PMT_NIL);
    // iterate through subscribers on port
    while( pmt::pmt_is_pair(currlist) ){
        pmt::pmt_t target = pmt::pmt_car(currlist);

        pmt::pmt_t block = pmt::pmt_car(target);
        pmt::pmt_t port = pmt::pmt_cdr(target);
    
        currlist = pmt::pmt_cdr(currlist);
        gr_basic_block_sptr blk = global_block_registry.block_lookup(block);
        //blk->post(msg);
        blk->post(port, msg);
        }
    }

//  - subscribe to a message port
void gr_basic_block::message_port_sub(pmt::pmt_t port_id, pmt::pmt_t target){
    if(!pmt::pmt_dict_has_key(message_subscribers, port_id)){ 
        std::stringstream ss;
        ss << "Port does not exist: \"" << pmt::pmt_write_string(port_id) << "\" on block: " << pmt::pmt_write_string(target) << std::endl;
        throw std::runtime_error(ss.str());
    }
    pmt::pmt_t currlist = pmt::pmt_dict_ref(message_subscribers,port_id,pmt::PMT_NIL);
    message_subscribers = pmt::pmt_dict_add(message_subscribers,port_id,pmt::pmt_list_add(currlist,target));
    }

void gr_basic_block::message_port_unsub(pmt::pmt_t port_id, pmt::pmt_t target){
    if(!pmt::pmt_dict_has_key(message_subscribers, port_id)){ 
        std::stringstream ss;
        ss << "Port does not exist: \"" << pmt::pmt_write_string(port_id) << "\" on block: " << pmt::pmt_write_string(target) << std::endl;
        throw std::runtime_error(ss.str());
    }
    pmt::pmt_t currlist = pmt::pmt_dict_ref(message_subscribers,port_id,pmt::PMT_NIL);
    message_subscribers = pmt::pmt_dict_add(message_subscribers,port_id,pmt::pmt_list_rm(currlist,target));
    }

void
gr_basic_block::_post(pmt_t which_port, pmt_t msg)
{
  insert_tail(which_port, msg);
  //notify_msg();
}

void
gr_basic_block::insert_tail(pmt::pmt_t which_port, pmt::pmt_t msg)
{
  gruel::scoped_lock guard(mutex);

  msg_queue[which_port].push_back(msg);

  // wake up thread if BLKD_IN or BLKD_OUT
  //input_cond.notify_one();
  //output_cond.notify_one();
  // TODO: reconsider the need for notification of input and output conditions!
}

pmt_t
gr_basic_block::delete_head_nowait(pmt::pmt_t which_port)
{
  gruel::scoped_lock guard(mutex);

  if (empty_p(which_port))
    return pmt_t();

  pmt_t m(msg_queue[which_port].front());
  msg_queue[which_port].pop_front();

  return m;
}

/*
 * Caller must already be holding the mutex
 */
pmt_t
gr_basic_block::delete_head_nowait_already_holding_mutex(pmt::pmt_t which_port)
{
  if (empty_p(which_port))
    return pmt_t();

  pmt_t m(msg_queue[which_port].front());
  msg_queue[which_port].pop_front();

  return m;
}

