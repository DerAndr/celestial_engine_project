package com.celestial.companion.ai

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class GemmaVariantRegistryTest {

    @Test
    fun `default model is GEMMA_4_E2B with expected OpenRouter modelId`() {
        assertEquals(GemmaVariant.GEMMA_4_E2B, GemmaVariant.DEFAULT)
        assertEquals(GemmaVariant.GEMMA_4_E2B, GemmaVariantRegistry.DEFAULT_VARIANT)
        assertEquals("google/gemma-4-e2b-it", GemmaVariant.DEFAULT.modelId)
        assertEquals(GemmaVariant.GEMMA_4_E2B, GemmaVariant.E2B_MULTIMODAL)
        assertEquals(GemmaVariant.GEMMA_4_26B, GemmaVariant.E4B_THINKING)
        assertEquals(GemmaVariant.GEMMA_3_27B, GemmaVariant.FLASH_NANO)
    }

    @Test
    fun `all variants have non-blank modelId, displayName, and shortTag`() {
        val variants = GemmaVariantRegistry.allVariants()
        assertTrue(variants.isNotEmpty())
        val seenIds = mutableSetOf<Int>()
        val seenTags = mutableSetOf<String>()

        for (variant in variants) {
            assertTrue("modelId must not be blank for $variant", variant.modelId.isNotBlank())
            assertTrue("displayName must not be blank for $variant", variant.displayName.isNotBlank())
            assertTrue("shortTag must not be blank for $variant", variant.shortTag.isNotBlank())
            assertTrue("tag must not be blank for $variant", variant.tag.isNotBlank())
            assertEquals(variant.tag, variant.shortTag)
            assertTrue("description must not be blank for $variant", variant.description.isNotBlank())
            assertTrue("fallbackModelId must not be blank for $variant", variant.fallbackModelId.isNotBlank())
            assertTrue("Duplicate id ${variant.id}", seenIds.add(variant.id))
            assertTrue("Duplicate shortTag ${variant.shortTag}", seenTags.add(variant.shortTag))
        }
    }

    @Test
    fun `lookup and registry helper methods work as expected`() {
        // ID lookup
        assertEquals(GemmaVariant.GEMINI_FLASH, GemmaVariantRegistry.findById(0))
        assertEquals(GemmaVariant.GEMMA_4_E2B, GemmaVariantRegistry.findById(1))
        assertEquals(GemmaVariant.GEMMA_4_31B, GemmaVariantRegistry.findById(2))
        assertEquals(GemmaVariant.GEMMA_4_26B, GemmaVariantRegistry.findById(3))
        assertEquals(GemmaVariant.GEMMA_3_27B, GemmaVariantRegistry.findById(4))
        assertEquals(GemmaVariant.PI5_OFFLOAD, GemmaVariantRegistry.findById(5))
        assertNull(GemmaVariantRegistry.findById(-1))
        assertNull(GemmaVariantRegistry.findById(999))

        // modelId lookup (case-insensitive & trimmed)
        assertEquals(
            GemmaVariant.GEMMA_4_E2B,
            GemmaVariantRegistry.findByModelId("google/gemma-4-e2b-it")
        )
        assertEquals(
            GemmaVariant.GEMMA_4_26B,
            GemmaVariantRegistry.findByModelId("  GOOGLE/GEMMA-4-26B-A4B-IT ")
        )
        assertEquals(
            GemmaVariant.PI5_OFFLOAD,
            GemmaVariantRegistry.findByModelId("local")
        )
        assertNull(GemmaVariantRegistry.findByModelId(""))
        assertNull(GemmaVariantRegistry.findByModelId("unknown/model"))

        // shortTag / tag lookup
        assertEquals(GemmaVariant.GEMMA_4_E2B, GemmaVariantRegistry.findByTag("G4-E2B"))
        assertEquals(GemmaVariant.GEMINI_FLASH, GemmaVariantRegistry.findByTag("g2.5-fl"))
        assertNull(GemmaVariantRegistry.findByTag(" "))
        assertNull(GemmaVariantRegistry.findByTag("UNKNOWN"))

        // Name lookup with fallback default
        assertEquals(GemmaVariant.GEMMA_4_31B, GemmaVariantRegistry.fromNameOrDefault("GEMMA_4_31B"))
        assertEquals(GemmaVariant.GEMMA_4_E2B, GemmaVariantRegistry.fromNameOrDefault(null))
        assertEquals(GemmaVariant.GEMMA_4_E2B, GemmaVariantRegistry.fromNameOrDefault(""))
        assertEquals(GemmaVariant.GEMMA_4_E2B, GemmaVariantRegistry.fromNameOrDefault("INVALID_VARIANT"))
        assertEquals(
            GemmaVariant.GEMINI_FLASH,
            GemmaVariantRegistry.fromNameOrDefault("INVALID_VARIANT", default = GemmaVariant.GEMINI_FLASH)
        )

        // Multimodal filter
        val multimodal = GemmaVariantRegistry.multimodalVariants()
        assertTrue(multimodal.contains(GemmaVariant.GEMMA_4_E2B))
        assertFalse(multimodal.contains(GemmaVariant.PI5_OFFLOAD))

        // Local weights file mapping
        assertEquals(
            "gemma-4-e2b-it.bin",
            GemmaVariantRegistry.localModelFileName(GemmaVariant.GEMMA_4_E2B)
        )
        assertEquals(
            "gemma-4-e4b-it.bin",
            GemmaVariantRegistry.localModelFileName(GemmaVariant.GEMMA_4_26B)
        )
        assertEquals(
            "generic-model.bin",
            GemmaVariantRegistry.localModelFileName(GemmaVariant.GEMINI_FLASH)
        )
    }
}
