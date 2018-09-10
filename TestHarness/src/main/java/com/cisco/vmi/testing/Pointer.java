/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package com.cisco.vmi.testing;

/**
 *
 * @author eshalev
 */
public class Pointer<T> {

    private T m_Value;
    public void set(T value){
        m_Value=value;
    }
    public T get(){
        return m_Value;
    }
}
