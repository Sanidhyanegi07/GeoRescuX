package com.example.georescux.data.relay

import org.junit.Assert.assertEquals
import org.junit.Test

class SQLiteSeenEventStoreEscapeTest {

    @Test
    fun `standard alphanumeric id is unchanged`() {
        assertEquals("alert-12345", SQLiteSeenEventStore.escapeLikePattern("alert-12345"))
    }

    @Test
    fun `underscore is escaped with backslash`() {
        assertEquals("alert\\_12345", SQLiteSeenEventStore.escapeLikePattern("alert_12345"))
    }

    @Test
    fun `percent is escaped with backslash`() {
        assertEquals("alert\\%12345", SQLiteSeenEventStore.escapeLikePattern("alert%12345"))
    }

    @Test
    fun `backslash is escaped with double backslash`() {
        assertEquals("alert\\\\12345", SQLiteSeenEventStore.escapeLikePattern("alert\\12345"))
    }

    @Test
    fun `complex mixed wildcards are safely escaped`() {
        assertEquals("ev\\_ent\\%1\\\\seq", SQLiteSeenEventStore.escapeLikePattern("ev_ent%1\\seq"))
    }
}
