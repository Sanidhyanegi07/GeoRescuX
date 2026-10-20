package com.example.georescux.ui.incident

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.graphics.Rect
import android.graphics.drawable.BitmapDrawable
import android.graphics.drawable.Drawable
import androidx.core.content.ContextCompat
import com.example.georescux.R
import com.example.georescux.data.incident.IncidentReport

/**
 * Creates custom pin markers for osmdroid MapView.
 * Provides distinct, high-contrast, visually appealing map pins with incident-specific emojis and colors.
 */
object IncidentIconFactory {

    private val iconCache = mutableMapOf<String, Drawable>()

    fun getMarkerIcon(context: Context, incident: IncidentReport): Drawable {
        val cacheKey = "${incident.type.uppercase()}_${incident.severity.uppercase()}"
        iconCache[cacheKey]?.let { return it }

        val density = context.resources.displayMetrics.density
        val width = (42 * density).toInt()
        val height = (50 * density).toInt()

        val bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(bitmap)

        val primaryColor = getPrimaryColor(incident.type)
        val radius = width / 2f - (2 * density)

        // 1. Draw Pin Body (circle on top with triangle pointing down)
        val path = Path().apply {
            // Pin point at bottom center
            moveTo(width / 2f, height - (2 * density))
            lineTo(width / 2f - radius * 0.7f, radius + (6 * density))
            arcTo(
                (2 * density),
                (2 * density),
                width - (2 * density),
                radius * 2f + (2 * density),
                150f,
                240f,
                false
            )
            lineTo(width / 2f, height - (2 * density))
            close()
        }

        // Drop shadow / border
        val borderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Color.WHITE
            style = Paint.Style.STROKE
            strokeWidth = 3 * density
            strokeJoin = Paint.Join.ROUND
        }
        canvas.drawPath(path, borderPaint)

        // Fill Pin
        val fillPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = primaryColor
            style = Paint.Style.FILL
        }
        canvas.drawPath(path, fillPaint)

        // Inner white circle for emoji background
        val innerCirclePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Color.WHITE
            style = Paint.Style.FILL
        }
        canvas.drawCircle(width / 2f, radius + (2 * density), radius * 0.75f, innerCirclePaint)

        // Draw Emoji in center of circle
        val emoji = incident.getTypeEmoji()
        val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            textSize = 15 * density
            textAlign = Paint.Align.CENTER
        }

        val textBounds = Rect()
        textPaint.getTextBounds(emoji, 0, emoji.length, textBounds)
        val textY = radius + (2 * density) + (textBounds.height() / 2f) - (textBounds.bottom)
        canvas.drawText(emoji, width / 2f, textY, textPaint)

        val drawable = BitmapDrawable(context.resources, bitmap)
        iconCache[cacheKey] = drawable
        return drawable
    }

    fun getPrimaryColor(type: String): Int {
        return when (type.uppercase()) {
            "FLOOD" -> 0xFF0284C7.toInt() // Sky Blue
            "EARTHQUAKE" -> 0xFFD97706.toInt() // Earth Amber
            "FIRE" -> 0xFFDC2626.toInt() // Red
            "LANDSLIDE" -> 0xFF7C3AED.toInt() // Violet / Mountain Stone
            "ROUTE_BLOCK", "ROAD_BLOCK" -> 0xFFEA580C.toInt() // Hazard Orange
            "BUILDING_COLLAPSE", "COLLAPSE" -> 0xFF475569.toInt() // Slate Gray
            "MEDICAL" -> 0xFFE11D48.toInt() // Rose Red
            else -> 0xFFEA580C.toInt() // Amber / Hazard Orange
        }
    }
}
