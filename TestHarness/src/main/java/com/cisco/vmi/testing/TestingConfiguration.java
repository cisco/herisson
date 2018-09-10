/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package com.cisco.vmi.testing;

import java.util.HashMap;
import java.util.Map;
import java.util.Vector;

/**
 *
 * @author eshalev
 */
public class TestingConfiguration {

    Map<String, String> m_ConfigurationValues = new HashMap<>();

    public void set(String key, String value) {
        m_ConfigurationValues.put(key, value);
    }

    public String get(String key) {
        if (!m_ConfigurationValues.containsKey(key)) {
            throw new IndexOutOfBoundsException(key + " not configured");
        }
        return m_ConfigurationValues.get(key);
    }

    public String get(String key, String defaultValue) {
        return m_ConfigurationValues.getOrDefault(key, defaultValue);
    }

    Map<String, String> m_PrependEnvironement = new HashMap<>();

    public void prependEnvironmentVariable(String key, String value) {
        if (!m_PrependEnvironement.containsKey(key)) {
            m_PrependEnvironement.put(key, value);
            return;
        } else {
            m_PrependEnvironement.put(key, value + m_PrependEnvironement.get(key));
        }
    }

}
