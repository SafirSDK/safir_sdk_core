// -*- coding: utf-8 -*-
/******************************************************************************
*
* Copyright Saab AB, 2008-2013 (http://safirsdkcore.com)
*
* Created by: Henrik Sundberg / sthesu
*
*******************************************************************************
*
* This file is part of Safir SDK Core.
*
* Safir SDK Core is free software: you can redistribute it and/or modify
* it under the terms of version 3 of the GNU General Public License as
* published by the Free Software Foundation.
*
* Safir SDK Core is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with Safir SDK Core.  If not, see <http://www.gnu.org/licenses/>.
*
******************************************************************************/

package com.saabgroup.safir.dob.typesystem;

import java.io.UnsupportedEncodingException;

/**
 * Class containing the identity of a channel.
 */
public class ChannelId implements Comparable<ChannelId> {

    /** Constant representing all channels. */
    public static final ChannelId ALL_CHANNELS = new ChannelId("ALL_CHANNELS");

    /**
     * Default constructor.
     *
     * Creates a default channel id.
     */
    public ChannelId(){
        this.m_channelId = Kernel.Generate64("DEFAULT_CHANNEL");
        this.m_channelIdStr = "DEFAULT_CHANNEL";
    }

    /**
     * Constructor.
     *
     * Creates a channel id from the given string.
     *
     * @param id String identifying the channel.
     */
    public ChannelId(String id){
        if(id == null || id.length() == 0){
            throw new com.saabgroup.safir.dob.typesystem.SoftwareViolationException("ChannelId can not be generated from null/empty string");
        }
        this.m_channelId = Kernel.Generate64(id);
        this.m_channelIdStr = id;
    }

    /**
     * Constructor.
     *
     * Creates a channel id from the given id.
     *
     * @param id Identifier identifying the channel.
     */
    public ChannelId(long id){
        this.m_channelId = id;
        this.m_channelIdStr = "";
    }

    /**
     * Constructor.
     *
     * Creates a channel id from the given data.
     *
     * @param id [in] - Identifier identifying the channel.
     * @param idStr [in] - String identifying the channel.
     */
    public ChannelId(long id, String idStr){
        this.m_channelId = id;
        this.m_channelIdStr = idStr;
        if (idStr == null){
            throw new SoftwareViolationException("String argument to ChannelId constructor cannot be null");
        }
    }

    /**
     * Remove the included string from the channel id.
     *
     * This is meant to be used when this type is used as a member of a Dob object.
     * Using this call before the object gets serialized to binary or xml (i.e.
     * also before sending it anywhere) means that the string will not be included
     * when the object is sent.
     */
    public void removeString(){
        this.m_channelIdStr = "";
        m_cachedUtf8String = null;
    }

    /**
     * Equals.
     *
     * @param other The channel id to compare with.
     * @return True if the channel ids are equal.
     */
    public boolean equals(java.lang.Object other){
        if ((other == null) || !(other instanceof ChannelId)){
            return false;
        }
        else {
            return m_channelId == ((ChannelId)other).m_channelId;
        }
    }

    /**
     * Return a string representation of the channel id.
     *
     * @return String representation of the channel id.
     */
    public String toString(){
        if (m_channelIdStr.length() > 0){
            return m_channelIdStr;
        }
        else if (m_channelId == DEFAULT_CHANNEL.getRawValue()){
            return DEFAULT_CHANNEL.getRawString();
        }
        else if (m_channelId == ALL_CHANNELS.getRawValue()){
            return ALL_CHANNELS.getRawString();
        }
        else{
            return java.lang.Long.toString(m_channelId);
        }
    }

    /**
     * Overridden base class method.
     *
     * @return Hash code.
     */
    public int hashCode(){
        return (int)m_channelId;
    }

    /**
     * Get the raw 64 bit integer identifier.
     *
     * @return The raw 64 bit identifier.
     */
    public long getRawValue(){
        return m_channelId;
    }

    /**
     * Get the string that was used to create this id.
     *
     * If no string was used this method returns an empty string.
     *
     * @return The string (if any) that was used to create this id.
     */
    public String getRawString(){
        return m_channelIdStr;
    }

    /**
     * Get the length of the string when converted to UTF-8 encoding.
     * Includes one byte for a null termination.
     *
     * @return The length of the string of the id when converted to UTF-8
     */
    public int utf8StringLength() {
        if (m_channelIdStr.length() == 0) {
            return 0;
        }

        if (m_cachedUtf8String == null) {
            try {
                m_cachedUtf8String = m_channelIdStr.getBytes("UTF-8");
            } catch (UnsupportedEncodingException e) {
                throw new SoftwareViolationException("Failed to convert string to UTF-8!!!");
            }
        }

        return m_cachedUtf8String.length + 1;
    }

    /**
     * Convert the string to UTF-8.
     *
     * Returns an empty string if there is no string.
     *
     * @return UTF-8 representation of the string.
     */
    public byte [] utf8String() {
        if (m_cachedUtf8String == null) {
            try {
                m_cachedUtf8String = m_channelIdStr.getBytes("UTF-8");
            } catch (UnsupportedEncodingException e) {
                throw new SoftwareViolationException("Failed to convert string to UTF-8!!!");
            }
        }
        return m_cachedUtf8String;
    }
    
    /**
     * Compares two instances
     * @param other The object to compare to.
     * @return -1 if this instance is less than other, 1 if this is bigger, else 0.
     */
    @Override
    public int compareTo(ChannelId other) {
        if (getRawValue()<other.getRawValue())
            return -1;
        else if (getRawValue()>other.getRawValue())
            return 1;
        else
            return 0;
    }

    private long m_channelId = -1;
    private String m_channelIdStr;
    private byte [] m_cachedUtf8String;
    private static final ChannelId DEFAULT_CHANNEL  = new ChannelId();
}
