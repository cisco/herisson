/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package com.cisco.vmi.testing;

import java.util.HashSet;
import java.util.Set;
import java.util.function.Consumer;

/**
 *
 * @author eshalev
 */
public class EventPublisher<T> {

    Set<Consumer<T>> m_Consumers = new HashSet<>();

    public synchronized Consumer<T> subscribe(Consumer<T> consumer) {
        m_Consumers.add(consumer);
        return consumer;
    }

    public synchronized void publish(T event) {
        m_Consumers.forEach((consumer) -> consumer.accept(event));
    }

    public synchronized Consumer<T> unSubscribe(Consumer<T> consumer) {
        m_Consumers.remove(consumer);
        return consumer;
    }

    public synchronized Consumer<T> pipeTo(EventPublisher<T> publisher) {
        Consumer<T> consumer = (T t) -> publisher.publish(t);
        return subscribe(consumer);
    }

}
