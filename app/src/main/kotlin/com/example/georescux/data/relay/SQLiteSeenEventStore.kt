package com.example.georescux.data.relay

import android.content.ContentValues
import android.content.Context
import android.database.sqlite.SQLiteDatabase
import android.database.sqlite.SQLiteOpenHelper

/**
 * SQLite single-entity database implementation of [SeenEventStore].
 * Persists seen event IDs durably to disk so deduplication state survives process death.
 */
class SQLiteSeenEventStore(context: Context) :
    SQLiteOpenHelper(context.applicationContext, DB_NAME, null, DB_VERSION),
    SeenEventStore,
    RelayOutboundQueue {

    override fun onCreate(db: SQLiteDatabase) {
        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS $TABLE_SEEN_EVENTS (
                $COL_EVENT_ID TEXT PRIMARY KEY,
                $COL_SEEN_AT INTEGER NOT NULL
            )
            """.trimIndent()
        )
        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS $TABLE_OUTBOUND_ENVELOPES (
                $COL_OUT_EVENT_ID TEXT PRIMARY KEY,
                $COL_OUT_JSON TEXT NOT NULL,
                $COL_OUT_QUEUED_AT INTEGER NOT NULL
            )
            """.trimIndent()
        )
    }

    override fun onUpgrade(db: SQLiteDatabase, oldVersion: Int, newVersion: Int) {
        if (oldVersion < 2) {
            db.execSQL(
                """
                CREATE TABLE IF NOT EXISTS $TABLE_OUTBOUND_ENVELOPES (
                    $COL_OUT_EVENT_ID TEXT PRIMARY KEY,
                    $COL_OUT_JSON TEXT NOT NULL,
                    $COL_OUT_QUEUED_AT INTEGER NOT NULL
                )
                """.trimIndent()
            )
        }
    }

    override fun loadAllSeenEventIds(): Set<String> {
        val db = readableDatabase
        val ids = mutableSetOf<String>()
        val cursor = db.query(
            TABLE_SEEN_EVENTS,
            arrayOf(COL_EVENT_ID),
            null,
            null,
            null,
            null,
            null
        )
        cursor.use {
            while (it.moveToNext()) {
                ids.add(it.getString(0))
            }
        }
        return ids
    }

    override fun saveSeenEventId(eventId: String, timestampMs: Long) {
        val db = writableDatabase
        val values = ContentValues().apply {
            put(COL_EVENT_ID, eventId)
            put(COL_SEEN_AT, timestampMs)
        }
        db.insertWithOnConflict(TABLE_SEEN_EVENTS, null, values, SQLiteDatabase.CONFLICT_IGNORE)
    }

    override fun clearExpiredBefore(cutoffMs: Long) {
        val db = writableDatabase
        db.delete(TABLE_SEEN_EVENTS, "$COL_SEEN_AT < ?", arrayOf(cutoffMs.toString()))
    }

    // --- RelayOutboundQueue ---

    override fun loadQueued(): List<com.example.georescux.domain.relay.RelayEnvelope> {
        val db = readableDatabase
        val rows = mutableListOf<com.example.georescux.domain.relay.RelayEnvelope>()
        db.query(
            TABLE_OUTBOUND_ENVELOPES,
            arrayOf(COL_OUT_JSON),
            null,
            null,
            null,
            null,
            "$COL_OUT_QUEUED_AT ASC"
        ).use { cursor ->
            while (cursor.moveToNext()) {
                val json = cursor.getString(0)
                RelayEnvelopeSerializer.deserialize(json)?.let { rows.add(it) }
            }
        }
        return rows
    }

    override fun enqueue(envelope: com.example.georescux.domain.relay.RelayEnvelope, queuedAtMs: Long) {
        val json = RelayEnvelopeSerializer.serialize(envelope)
        val key = if (envelope.event.sequence > 0L) "${envelope.event.eventId}:${envelope.event.sequence}" else envelope.event.eventId
        val values = ContentValues().apply {
            put(COL_OUT_EVENT_ID, key)
            put(COL_OUT_JSON, json)
            put(COL_OUT_QUEUED_AT, queuedAtMs)
        }
        writableDatabase.insertWithOnConflict(
            TABLE_OUTBOUND_ENVELOPES,
            null,
            values,
            SQLiteDatabase.CONFLICT_REPLACE
        )
    }

    override fun remove(eventId: String) {
        writableDatabase.delete(
            TABLE_OUTBOUND_ENVELOPES,
            "$COL_OUT_EVENT_ID = ? OR $COL_OUT_EVENT_ID LIKE ?",
            arrayOf(eventId, "$eventId:%")
        )
    }

    companion object {
        const val DB_NAME = "georescux_relay_seen_events.db"
        const val DB_VERSION = 2

        const val TABLE_SEEN_EVENTS = "seen_events"
        const val COL_EVENT_ID = "event_id"
        const val COL_SEEN_AT = "seen_at_ms"

        const val TABLE_OUTBOUND_ENVELOPES = "relay_outbound_envelopes"
        const val COL_OUT_EVENT_ID = "event_id"
        const val COL_OUT_JSON = "envelope_json"
        const val COL_OUT_QUEUED_AT = "queued_at_ms"
    }
}
