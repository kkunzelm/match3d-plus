# Match3D+ STL-Unterstützung – Implementierungsplan

**Datum:** 2026-06-02
**Ziel:** Erweiterung von Match3D+ um native 3D-STL-Verarbeitung für Verschleißmessung

---

## 1. Übersicht

### 1.1 Ausgangslage

| Aspekt | Match3D+ (aktuell) | DentScanCompare (Referenz) |
|--------|-------------------|---------------------------|
| Datenformat | VIFF/XV (2.5D Höhenbild) | STL (3D Mesh) |
| Qt-Version | Qt 6.4+ | Qt 5.15 |
| Visualisierung | Qt Widgets (QPainter) | VTK 9.3 |
| Mesh-Bibliothek | – | CGAL 6.0.1 |
| ICP | CCCoreLib (Punktwolke) | nanoflann + Eigen |

### 1.2 Ziel-Architektur

Match3D+ wird um einen **parallelen 3D-Workflow** erweitert:

```
                    ┌─────────────────────────────────────────┐
                    │            Match3D+ Hauptfenster         │
                    ├───────────────────┬─────────────────────┤
                    │   2.5D Workflow   │    3D Workflow      │
                    │   (VIFF/PLY)      │    (STL)            │
                    ├───────────────────┼─────────────────────┤
                    │ ImageWindow       │ MeshWindow (neu)    │
                    │ DepthImageView    │ VTKMeshWidget (neu) │
                    │ ROI: Pixel-Maske  │ ROI: Vertex-Maske   │
                    │ ICP: CCCoreLib    │ ICP: nanoflann      │
                    └───────────────────┴─────────────────────┘
```

### 1.3 Verfügbare Abhängigkeiten

| Bibliothek | Version | Pfad |
|------------|---------|------|
| VTK | 9.3 (Qt6-Build) | ~/VTK-install-linux |
| CGAL | 6.0.1 | System (/usr/include/CGAL) |
| Eigen | 3.4.0 | System |
| nanoflann | 1.7.1 | System (/usr/include/nanoflann.hpp) |

---

## 2. Zu portierende Komponenten aus DentScanCompare

### 2.1 Core-Komponenten (direkt übernehmen)

| Datei | Funktion | Anpassungen |
|-------|----------|-------------|
| `Mesh.h` | ScanData-Struktur, CGAL-Typen | Keine |
| `STLReader.{h,cpp}` | Binary STL → CGAL SurfaceMesh | Import-Dialog mit Achsen-Optionen |
| `ICPRegistration.{h,cpp}` | Point-to-plane ICP | Keine |
| `DistanceField.{h,cpp}` | Signierte Abstände via AABB | Statistik-Erweiterung für Volumen |
| `CurvatureAnalysis.{h,cpp}` | κ_H, κ_G Berechnung | Keine |
| `ToothSegmentation.{h,cpp}` | Dijkstra-basierte ROI | Keine |

### 2.2 Visualisierung (neu implementieren für Qt6)

| Komponente | Quelle | Anpassung |
|------------|--------|-----------|
| `VTKMeshWidget` | DentScanCompare | Qt6 QVTKOpenGLNativeWidget |
| `ColorMapLUT` | DentScanCompare | Keine |

### 2.3 Nicht benötigt

- `GPAReference` (nur für Scanner-Vergleich, nicht Verschleiß)
- `TessellationMetrics`, `ArchMetrics` (Scanner-Qualität)
- `ScatterPlotWidget`, `MetricsTableWidget` (Scanner-Vergleich UI)

---

## 3. Implementierungsphasen

### Phase 1: Build-System & Abhängigkeiten

**Ziel:** CMake-Integration von VTK, CGAL, Eigen, nanoflann

**Aufgaben:**

1. **CMakeLists.txt (Root) erweitern:**
   ```cmake
   # VTK mit Qt6-Support
   set(VTK_DIR "$ENV{HOME}/VTK-install-linux/lib/cmake/vtk-9.3")
   find_package(VTK 9.3 REQUIRED COMPONENTS
       CommonCore
       CommonDataModel
       FiltersSources
       InteractionStyle
       RenderingCore
       RenderingOpenGL2
       GUISupportQt
   )

   # CGAL
   find_package(CGAL 6.0 REQUIRED)

   # Eigen
   find_package(Eigen3 3.4 REQUIRED)
   ```

2. **src/CMakeLists.txt erweitern:**
   ```cmake
   # ── 3D/STL Library ───────────────────────────────────────────────
   add_library(mesh3d_core STATIC
       mesh3d/Mesh.h
       mesh3d/STLReader.h
       mesh3d/STLReader.cpp
       mesh3d/ICPRegistration.h
       mesh3d/ICPRegistration.cpp
       mesh3d/DistanceField.h
       mesh3d/DistanceField.cpp
       mesh3d/CurvatureAnalysis.h
       mesh3d/CurvatureAnalysis.cpp
       mesh3d/ToothSegmentation.h
       mesh3d/ToothSegmentation.cpp
   )
   target_link_libraries(mesh3d_core
       PUBLIC CGAL::CGAL Eigen3::Eigen
   )
   target_include_directories(mesh3d_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

   # Main app: VTK hinzufügen
   target_link_libraries(match3d_plus PRIVATE mesh3d_core ${VTK_LIBRARIES})
   vtk_module_autoinit(TARGETS match3d_plus MODULES ${VTK_LIBRARIES})
   ```

3. **Verzeichnisstruktur anlegen:**
   ```
   src/
   ├── mesh3d/           (NEU)
   │   ├── Mesh.h
   │   ├── STLReader.{h,cpp}
   │   ├── ICPRegistration.{h,cpp}
   │   ├── DistanceField.{h,cpp}
   │   ├── CurvatureAnalysis.{h,cpp}
   │   └── ToothSegmentation.{h,cpp}
   ├── visualization3d/  (NEU)
   │   ├── VTKMeshWidget.{h,cpp}
   │   └── ColorMapLUT.{h,cpp}
   └── dialogs/
       └── STLImportDialog.{h,cpp}  (NEU)
   ```

**Verifizierung:**
- `cmake -B build && cmake --build build` kompiliert ohne Fehler
- VTK- und CGAL-Header werden gefunden

---

### Phase 2: STL-Import mit Achsenkorrektur

**Ziel:** STL-Dateien laden mit manueller Orientierungskorrektur

**Aufgaben:**

1. **`STLReader` aus DentScanCompare portieren:**
   - Datei: `src/mesh3d/STLReader.{h,cpp}`
   - Binary STL → CGAL `SurfaceMesh`
   - Winding-Korrektur (Primescan-Problem) beibehalten

2. **`STLImportDialog` erstellen:**
   ```cpp
   class STLImportDialog : public QDialog {
       // Vorschau-Widget (einfache 2D-Projektion oder Mini-VTK)
       // Achsen-Transformation:
       QComboBox* m_axisMapping;    // XYZ, XZY, YXZ, YZX, ZXY, ZYX
       QCheckBox* m_flipX;
       QCheckBox* m_flipY;
       QCheckBox* m_flipZ;
       QCheckBox* m_invertNormals;
       // Live-Vorschau bei Änderung
   };
   ```

3. **Transformationslogik:**
   ```cpp
   Eigen::Matrix4d buildImportTransform(
       AxisMapping mapping,
       bool flipX, bool flipY, bool flipZ,
       bool invertNormals
   );
   ```

4. **MainWindow-Integration:**
   - Menü: File → Open STL...
   - Dialog öffnen, Transformation anwenden, `ScanData` erzeugen

**Verifizierung:**
- STL-Datei laden (z.B. aus ~/claude-code/match3d-plus/data/3d-data/stl/)
- Achsen-Flip funktioniert in Vorschau
- Mesh-Statistik (Vertices, Faces, Bounds) korrekt

---

### Phase 3: 3D-Visualisierung (VTKMeshWidget)

**Ziel:** Interaktive 3D-Darstellung von Meshes

**Aufgaben:**

1. **`VTKMeshWidget` aus DentScanCompare portieren:**
   - Datei: `src/visualization3d/VTKMeshWidget.{h,cpp}`
   - Qt6 `QVTKOpenGLNativeWidget` verwenden
   - Funktionen:
     - `setMesh(ScanData&)` – einzelnes Mesh anzeigen
     - `setOverlayMeshes(...)` – mehrere semi-transparent
     - `showDistanceMap(min, max, mask)` – Falschfarben
     - `setPickMode(bool)` – Punkte auf Oberfläche wählen

2. **`ColorMapLUT` portieren:**
   - Divergierende Farbskala (blau–weiß–rot) für Differenzen
   - Grauskala für Mesh-Darstellung

3. **`MeshWindow` erstellen (analog zu `ImageWindow`):**
   ```cpp
   class MeshWindow : public QWidget {
       VTKMeshWidget* m_meshView;
       QToolBar* m_toolbar;   // Style-Dropdown, ROI-Buttons
       QStatusBar* m_status;  // Vertex-Koordinaten bei Hover

       std::shared_ptr<ScanData> m_scan;
       std::vector<bool> m_roiMask;  // Vertex-basiert
   };
   ```

4. **MainWindow: MDI-Integration:**
   - `MeshWindow` als MDI-Child (analog zu `ImageWindow`)
   - Bildliste unterscheidet 2D/3D-Fenster (Icon oder Suffix)

**Verifizierung:**
- STL öffnen → MeshWindow erscheint mit 3D-Ansicht
- Maus-Rotation, Zoom funktionieren
- Statusleiste zeigt Koordinaten bei Hover

---

### Phase 4: ROI-Auswahl für 3D

**Ziel:** Vertex-basierte Selektion (analog zu Pixel-ROI)

**Aufgaben:**

1. **`ToothSegmentation` portieren:**
   - Dijkstra-basierte Region Growing
   - Seed-Punkte durch Klick auf Mesh
   - Parameter: maxGeodesicMm, maxCreaseAngle, minMeanCurvature

2. **`CurvatureAnalysis` portieren:**
   - CGAL `interpolated_corrected_curvatures`
   - Voraussetzung für ToothSegmentation

3. **MeshWindow ROI-Menü:**
   ```
   Edit → Select all
   Edit → Unselect all
   Edit → Select by seed points...  (öffnet Seed-Picker-Modus)
   Edit → Unselect polygon (3D-Lasso? Oder 2D-Projektion)
   Edit → Commit selection
   ```

4. **Vereinfachte 3D-Polygon-Auswahl:**
   - Option A: Projektion auf Bildschirmebene, dann Raycast
   - Option B: Nur Seed-basierte Segmentation (kein Polygon)

   **Empfehlung:** Zunächst nur Seed-basiert (Option B), Polygon später

**Verifizierung:**
- Klick auf Mesh → Seed setzen
- Segmentation läuft, Zahnkronen werden farbig markiert
- Select all / Unselect all funktioniert

---

### Phase 5: 3D-Registrierung

**Ziel:** ICP-Alignment zweier STL-Meshes

**Aufgaben:**

1. **`ICPRegistration` portieren:**
   - Point-to-plane ICP mit nanoflann KD-Tree
   - Parameter: maxIterations, convergenceRms, sampleCount, maxCorrespDist

2. **Registrierungs-Workflow im GUI:**
   - Zwei Meshes laden (Referenz + Daten)
   - Coarse: From COM (Schwerpunkte matchen)
   - Fine: ICP mit Fortschrittsanzeige
   - ROI-Maske berücksichtigen (nur selektierte Vertices)

3. **MatchingControlPanel erweitern:**
   - Erkennen ob 2D- oder 3D-Fenster aktiv
   - Bei 3D: ICPRegistration statt CCCoreLib verwenden
   - Parameter-Spinboxen anpassen

4. **Auto-Matching für 3D:**
   - Analog zu 2.5D: Ausreißer-Filterung bei negativen Differenzen
   - In ICPRegistration.cpp: Korrespondenzen mit diff < -threshold ausschließen

**Verifizierung:**
- Zwei STL laden, ICP starten
- Fortschrittsanzeige funktioniert
- Finale RMS plausibel

---

### Phase 6: Differenzberechnung & Statistik

**Ziel:** Identische Statistik-Ausgabe wie bei 2.5D

**Aufgaben:**

1. **`DistanceField` portieren:**
   - CGAL AABB-Tree für Closest-Point-Queries
   - Signierte Distanz: `dot(diff, face_normal)`

2. **Statistik-Berechnung (flächengewichtet):**
   ```cpp
   struct MeshStatistics {
       int validVertices;
       double totalArea;      // mm²
       double mean, stdDev;
       double min, max;
       double q02, q05, q10, q50, q90, q95, q98;
       // Volumen (flächengewichtet):
       double posVolume;  // mm³ (Material über Referenz)
       double negVolume;  // mm³ (Verschleiß)
   };

   // Volumenberechnung:
   // Für jedes Dreieck: V = (z_avg * Fläche) / 3
   // (vereinfacht für quasi-planare Dreiecke)
   ```

3. **Statistik-Dialog für 3D:**
   - Identisches Format wie 2.5D (Process → Statistics...)
   - Speichern als Textdatei

4. **Differenz-Visualisierung:**
   - Neues MeshWindow mit Falschfarben
   - Rot = negativ (Verschleiß), Weiß = positiv

**Verifizierung:**
- Nach ICP: Diff-Image erzeugen
- Statistik zeigt plausible Werte
- Volumen-Berechnung: identisches Ergebnis bei synthetischen Testdaten

---

### Phase 7: UI-Vereinheitlichung & Polish

**Ziel:** Konsistente Benutzererfahrung für 2D und 3D

**Aufgaben:**

1. **Einheitliches Menü:**
   - File → Open... (erkennt automatisch VIFF/PLY/STL)
   - Oder: File → Open VIFF, File → Open STL (explizit)

2. **Bildlisten-Anzeige:**
   - Icon oder Tag für 2D vs. 3D
   - Drag & Drop zwischen Listen

3. **Matching Control Panel:**
   - Automatische Erkennung des Fenstertyps
   - Parameter entsprechend anpassen

4. **Dokumentation:**
   - User Manual für 3D-Workflow
   - Beispiel-Workflow: Zahnverschleiß-Messung mit STL

---

## 4. Abhängigkeiten zwischen Phasen

```
Phase 1 ──► Phase 2 ──► Phase 3
                │           │
                │           ▼
                │       Phase 4
                │           │
                ▼           ▼
            Phase 5 ◄───────┘
                │
                ▼
            Phase 6
                │
                ▼
            Phase 7
```

**Kritischer Pfad:** 1 → 2 → 3 → 5 → 6

Phase 4 (ROI) kann parallel zu Phase 5 entwickelt werden.

---

## 5. Geschätzter Aufwand

| Phase | Beschreibung | Aufwand |
|-------|--------------|---------|
| 1 | Build-System | 1 Tag |
| 2 | STL-Import | 2 Tage |
| 3 | VTK-Visualisierung | 3 Tage |
| 4 | ROI-Auswahl | 2 Tage |
| 5 | ICP-Registrierung | 2 Tage |
| 6 | Differenz & Statistik | 2 Tage |
| 7 | UI & Polish | 2 Tage |
| **Gesamt** | | **~14 Tage** |

---

## 6. Risiken & Mitigationen

| Risiko | Wahrscheinlichkeit | Mitigation |
|--------|-------------------|------------|
| VTK/Qt6 Inkompatibilität | Niedrig (bereits getestet) | Fallback: Qt5 oder OpenGL direkt |
| CGAL 6.0 API-Änderungen | Niedrig (Code aus DentScanCompare läuft) | Code bereits CGAL 6.0-kompatibel |
| Performance bei großen Meshes | Mittel | Subsampling für ICP, LOD für Visualisierung |
| 3D-Polygon-Auswahl komplex | Hoch | Zunächst nur Seed-basierte ROI |

---

## 7. Testdaten

Vorhandene STL-Testdaten:
```
~/claude-code/match3d-plus/data/3d-data/stl/
```

Falls nicht vorhanden, können Testdaten aus DentScanCompare kopiert werden:
```bash
cp -r ~/claude-code/DentScanCompare/data/stl ~/claude-code/match3d-plus/data/3d-data/
```

---

## 8. Nächste Schritte

1. **Bestätigung des Plans** durch Benutzer
2. **Phase 1 starten:** CMakeLists.txt erweitern, Build verifizieren
3. **Core-Dateien kopieren:** STLReader, ICPRegistration aus DentScanCompare

Soll ich mit Phase 1 beginnen?
