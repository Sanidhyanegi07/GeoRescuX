package com.example.georescux.data.incident

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Matrix
import android.net.Uri
import android.util.Base64
import android.media.ExifInterface
import java.io.ByteArrayOutputStream
import java.io.File
import java.io.FileOutputStream
import java.io.InputStream

/**
 * Robust helper for offline storage, thumbnailing, and compression of incident photos.
 * Ensures images survive app restarts and are protected against OutOfMemoryErrors.
 */
object ImageStorageHelper {

    private const val MAX_DIMENSION_PX = 1280
    private const val THUMBNAIL_DIMENSION_PX = 480
    private const val JPEG_QUALITY = 82
    private const val THUMBNAIL_QUALITY = 65

    /**
     * Creates a temporary file in cache for Camera capture via FileProvider.
     */
    fun createTempCameraFile(context: Context): File {
        val storageDir = File(context.cacheDir, "images").apply { mkdirs() }
        return File.createTempFile("camera_incident_", ".jpg", storageDir)
    }

    /**
     * Saves an image from a content Uri (Gallery or Camera) into internal storage
     * and returns the local file path along with an optional Base64 thumbnail string.
     */
    fun saveIncidentImage(
        context: Context,
        sourceUri: Uri,
        incidentId: String,
    ): Pair<String, String?> {
        val targetDir = File(context.filesDir, "incident_images").apply { mkdirs() }
        val targetFile = File(targetDir, "incident_${incidentId}.jpg")

        var bitmap: Bitmap? = null
        try {
            bitmap = decodeSampledBitmapFromUri(context, sourceUri, MAX_DIMENSION_PX, MAX_DIMENSION_PX)
            if (bitmap != null) {
                // Apply rotation from EXIF if needed
                val rotatedBitmap = rotateBitmapIfRequired(context, sourceUri, bitmap)
                FileOutputStream(targetFile).use { out ->
                    rotatedBitmap.compress(Bitmap.CompressFormat.JPEG, JPEG_QUALITY, out)
                    out.flush()
                }

                // Generate compact Base64 for cloud sync
                val base64Thumbnail = generateBase64Thumbnail(rotatedBitmap)
                return Pair(targetFile.absolutePath, base64Thumbnail)
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }

        // Fallback: direct stream copy if bitmap decode failed
        try {
            context.contentResolver.openInputStream(sourceUri)?.use { input ->
                FileOutputStream(targetFile).use { output ->
                    input.copyTo(output)
                }
            }
            return Pair(targetFile.absolutePath, null)
        } catch (e: Exception) {
            e.printStackTrace()
            return Pair(sourceUri.toString(), null)
        }
    }

    /**
     * Saves a Bitmap directly (e.g. from camera thumbnail preview) into internal storage.
     */
    fun saveIncidentBitmap(
        context: Context,
        bitmap: Bitmap,
        incidentId: String,
    ): Pair<String, String?> {
        val targetDir = File(context.filesDir, "incident_images").apply { mkdirs() }
        val targetFile = File(targetDir, "incident_${incidentId}.jpg")

        FileOutputStream(targetFile).use { out ->
            bitmap.compress(Bitmap.CompressFormat.JPEG, JPEG_QUALITY, out)
            out.flush()
        }

        val base64Thumbnail = generateBase64Thumbnail(bitmap)
        return Pair(targetFile.absolutePath, base64Thumbnail)
    }

    /**
     * Decodes a downsampled bitmap from a Uri to prevent OutOfMemory on high-res camera photos.
     */
    fun decodeSampledBitmapFromUri(
        context: Context,
        uri: Uri,
        reqWidth: Int,
        reqHeight: Int,
    ): Bitmap? {
        val options = BitmapFactory.Options().apply { inJustDecodeBounds = true }
        context.contentResolver.openInputStream(uri)?.use { stream ->
            BitmapFactory.decodeStream(stream, null, options)
        }

        options.inSampleSize = calculateInSampleSize(options, reqWidth, reqHeight)
        options.inJustDecodeBounds = false

        return context.contentResolver.openInputStream(uri)?.use { stream ->
            BitmapFactory.decodeStream(stream, null, options)
        }
    }

    /**
     * Loads a Bitmap from a local file path with requested bounds.
     */
    fun decodeSampledBitmapFromFile(
        filePath: String,
        reqWidth: Int = 800,
        reqHeight: Int = 800,
    ): Bitmap? {
        val file = File(filePath)
        if (!file.exists()) return null

        val options = BitmapFactory.Options().apply { inJustDecodeBounds = true }
        BitmapFactory.decodeFile(filePath, options)

        options.inSampleSize = calculateInSampleSize(options, reqWidth, reqHeight)
        options.inJustDecodeBounds = false

        return BitmapFactory.decodeFile(filePath, options)
    }

    /**
     * Decodes a Base64 string into a Bitmap.
     */
    fun decodeBase64(base64Str: String): Bitmap? {
        return try {
            val bytes = Base64.decode(base64Str, Base64.DEFAULT)
            BitmapFactory.decodeByteArray(bytes, 0, bytes.size)
        } catch (_: Exception) {
            null
        }
    }

    private fun generateBase64Thumbnail(source: Bitmap): String? {
        return try {
            val scale = minOf(
                THUMBNAIL_DIMENSION_PX.toFloat() / source.width,
                THUMBNAIL_DIMENSION_PX.toFloat() / source.height,
                1.0f
            )
            val scaled = if (scale < 1.0f) {
                Bitmap.createScaledBitmap(
                    source,
                    (source.width * scale).toInt(),
                    (source.height * scale).toInt(),
                    true
                )
            } else {
                source
            }

            val stream = ByteArrayOutputStream()
            scaled.compress(Bitmap.CompressFormat.JPEG, THUMBNAIL_QUALITY, stream)
            Base64.encodeToString(stream.toByteArray(), Base64.NO_WRAP)
        } catch (_: Exception) {
            null
        }
    }

    private fun calculateInSampleSize(
        options: BitmapFactory.Options,
        reqWidth: Int,
        reqHeight: Int,
    ): Int {
        val height = options.outHeight
        val width = options.outWidth
        var inSampleSize = 1

        if (height > reqHeight || width > reqWidth) {
            val halfHeight = height / 2
            val halfWidth = width / 2
            while ((halfHeight / inSampleSize) >= reqHeight && (halfWidth / inSampleSize) >= reqWidth) {
                inSampleSize *= 2
            }
        }
        return inSampleSize
    }

    private fun rotateBitmapIfRequired(context: Context, uri: Uri, bitmap: Bitmap): Bitmap {
        var input: InputStream? = null
        try {
            input = context.contentResolver.openInputStream(uri) ?: return bitmap
            val exif = ExifInterface(input)
            val orientation = exif.getAttributeInt(
                ExifInterface.TAG_ORIENTATION,
                ExifInterface.ORIENTATION_NORMAL
            )
            return when (orientation) {
                ExifInterface.ORIENTATION_ROTATE_90 -> rotateBitmap(bitmap, 90f)
                ExifInterface.ORIENTATION_ROTATE_180 -> rotateBitmap(bitmap, 180f)
                ExifInterface.ORIENTATION_ROTATE_270 -> rotateBitmap(bitmap, 270f)
                else -> bitmap
            }
        } catch (_: Exception) {
            return bitmap
        } finally {
            input?.close()
        }
    }

    private fun rotateBitmap(bitmap: Bitmap, degrees: Float): Bitmap {
        val matrix = Matrix().apply { postRotate(degrees) }
        return Bitmap.createBitmap(bitmap, 0, 0, bitmap.width, bitmap.height, matrix, true)
    }
}
