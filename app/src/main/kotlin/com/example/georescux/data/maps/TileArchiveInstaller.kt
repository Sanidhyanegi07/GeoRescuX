package com.example.georescux.data.maps

import android.content.Context
import com.example.georescux.domain.routing.MapRegion
import java.io.File
import java.io.FileOutputStream
import java.io.InputStream

/**
 * Copies the bundled offline tile or vector archive from assets into the osmdroid
 * base path (app-private storage), where osmdroid discovers and renders it.
 *
 * Enforces atomic extraction via temporary file, magic-byte validation,
 * and corruption self-healing.
 */
object TileArchiveInstaller {

    fun ensureExtracted(context: Context, region: MapRegion): File? = try {
        ensureExtracted(context.filesDir, { path -> context.assets.open(path) }, region)
    } catch (_: Exception) {
        null
    }

    fun ensureExtracted(
        filesDir: File,
        assetStreamProvider: (String) -> InputStream,
        region: MapRegion
    ): File? = try {
        if (region.tileAssetPath.isBlank()) null
        else {
            val extension = region.tileAssetPath.substringAfterLast('.', "map")
            val osmdroidDir = File(filesDir, "osmdroid")
            if (!osmdroidDir.exists()) osmdroidDir.mkdirs()

            val target = File(osmdroidDir, "${region.id}-tiles.$extension")

            // If target already exists, verify its integrity
            if (target.exists() && target.length() > 0L) {
                val isValid = when (extension) {
                    "map" -> MapPackageManager.isMapsforgeFile(target)
                    "sqlite" -> MapPackageManager.isSqliteArchiveFile(target)
                    else -> target.length() > 0L
                }
                if (isValid) {
                    target
                } else {
                    target.delete()
                    extractFromAssets(assetStreamProvider, region.tileAssetPath, target, extension)
                }
            } else {
                extractFromAssets(assetStreamProvider, region.tileAssetPath, target, extension)
            }
        }
    } catch (e: Exception) {
        null
    }

    private fun extractFromAssets(
        assetStreamProvider: (String) -> InputStream,
        assetPath: String,
        target: File,
        extension: String
    ): File? {
        val temp = File(target.parentFile, "${target.name}.tmp")
        return try {
            if (temp.exists()) temp.delete()

            assetStreamProvider(assetPath).use { input ->
                FileOutputStream(temp).use { output ->
                    input.copyTo(output)
                    output.flush()
                }
            }

            if (!temp.exists() || temp.length() == 0L) {
                temp.delete()
                null
            } else {
                // Verify format
                val isValid = when (extension) {
                    "map" -> MapPackageManager.isMapsforgeFile(temp)
                    "sqlite" -> MapPackageManager.isSqliteArchiveFile(temp)
                    else -> temp.length() > 0L
                }

                if (!isValid) {
                    temp.delete()
                    null
                } else {
                    // Atomic rename
                    if (target.exists()) target.delete()
                    if (!temp.renameTo(target)) {
                        temp.copyTo(target, overwrite = true)
                        temp.delete()
                    }

                    if (target.exists() && target.length() > 0L) target else null
                }
            }
        } catch (e: Exception) {
            if (temp.exists()) temp.delete()
            null
        }
    }
}
