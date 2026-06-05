# 3D-Registrierungsstrategien für match3d_v2

## Kontext

**Zweck:** Registrierung von Zahnoberflächen-Tiefenbildern zu verschiedenen Zeitpunkten zur Verschleißmessung.
- Baseline: Scan direkt nach Einsetzen der Restauration
- Follow-up: Scan nach Monaten/Jahren
- Ergebnis: Differenzbild = quantifizierter Materialverlust

**Entscheidungen für match3d_v2:**
- Primäre Feinregistrierung: ICP via CCCoreLib
- Großjustierung: Manuelle korrespondierende Punktepaare (wie Gloger/Neugebauer)
- Dateiformat: VIFF/XV + PLY
- Plattform: Linux primär

---

## 1. CCCoreLib ICP

### 1.1 Relevante Klassen

**`ICPRegistrationTools`** – Hauptklasse für ICP:
```cpp
// Hauptmethode:
static RESULT_TYPE Register(
    GenericIndexedCloudPersist* modelCloud,   // Referenz (fest)
    GenericIndexedMesh*         modelMesh,    // Alternative zum modelCloud
    GenericIndexedCloudPersist* dataCloud,    // Wird transformiert
    const Parameters&           params,
    bool                        inputDataIsFiltered,
    ScaledTransformation&       totalTrans,   // Output: Transformation
    double&                     finalRMS,     // Output: RMS-Fehler
    unsigned&                   finalPointCount
);
```

**`HornRegistrationTools`** – Für Grobjustierung mit korrespondierenden Punkten:
```cpp
static bool FindAbsoluteOrientation(
    GenericCloud* lCloud,        // Punktepaare Set 1
    GenericCloud* rCloud,        // Punktepaare Set 2
    ScaledTransformation& trans, // Output
    bool fixedScale = false
);
```

**`FPCSRegistrationTools`** – Bei sehr großen initialen Versätzen (nicht primär benötigt).

### 1.2 ICP-Parameter

```cpp
struct Parameters {
    // Konvergenz
    CONVERGENCE_TYPE convType = MAX_ERROR_CONVERGENCE;
    double  minRMSDecrease    = 1.0e-5;  // Stopp wenn (RMS_alt - RMS_neu)/RMS_neu < Schwelle
    unsigned nbMaxIterations  = 20;

    // Outlier
    bool    filterOutFarthestPoints = false; // Entferne Punkte > µ + 2.5σ

    // Datenreduktion
    unsigned samplingLimit    = 50000;   // Max. Punkte vor Subsampling
    double   finalOverlapRatio = 1.0;   // 1.0 = volle Überdeckung erwartet

    // Optional
    bool     adjustScale      = false;  // Skalierung mitschätzen
    ScalarField* modelWeights = nullptr;
    ScalarField* dataWeights  = nullptr;

    // Constraints
    int transformationFilters = SKIP_NONE; // Kann Rotation/Translation fixieren

    // Normals
    NORMALS_MATCHING normalsMatching = NO_NORMAL;

    // Mesh
    bool useC2MSignedDistances    = false;
    bool robustC2MSignedDistances = false;

    int maxThreadCount = 0; // 0 = alle CPUs
};
```

### 1.3 Transformation-Ausgabe

```cpp
struct ScaledTransformation {
    SquareMatrix R;   // 3×3 Rotationsmatrix
    CCVector3d   T;   // 3D-Translationsvektor
    PointCoordinateType s; // Skalierungsfaktor (1.0 wenn adjustScale=false)
};
// Anwendung: P_model = s * R * P_data + T
```

### 1.4 ICP-Algorithmus (Ablauf)

```
1. Initialisierung
   - Subsampling falls |dataCloud| > samplingLimit
   - KD-Baum auf modelCloud aufbauen

2. Hauptschleife (bis Konvergenz)
   a. Nächste Nachbarn finden (KD-Baum)
   b. Outlier entfernen (wenn filterOutFarthestPoints)
   c. Partial Overlap: nur beste (finalOverlapRatio × 100)% behalten
   d. RegistrationProcedure() aufrufen:
      - Schwerpunkte berechnen
      - Kovarianzmatrix Σ_px aufbauen
      - SVD / Jacobi → Rotation R
      - Translation T = G_model - R × G_data
   e. Transformation auf dataCloud anwenden
   f. RMS berechnen, Konvergenztest

3. Ergebnis: ScaledTransformation + finalRMS
```

### 1.5 Benötigte Headers

```cpp
#include <RegistrationTools.h>        // ICPRegistrationTools, HornRegistrationTools
#include <PointProjectionTools.h>     // Transformationsanwendung
#include <DistanceComputationTools.h> // Cloud-to-Cloud/-Mesh Distanzen
#include <GenericIndexedCloud.h>      // Datenstruktur für Punktwolken
#include <PointCloud.h>               // Konkrete Implementierung
```

### 1.6 Rückgabecodes

```cpp
enum RESULT_TYPE {
    ICP_NOTHING_TO_DO          = 0,
    ICP_APPLY_TRANSFO          = 1,   // Erfolg
    ICP_ERROR                  = 100,
    ICP_ERROR_REGISTRATION_STEP = 101,
    ICP_ERROR_DIST_COMPUTATION = 102,
    ICP_ERROR_NOT_ENOUGH_MEMORY = 103,
    ICP_ERROR_CANCELED_BY_USER = 104,
    ICP_ERROR_INVALID_INPUT    = 105
};
```

---

## 2. Gloger/Kunzelmann ICP (Java-Referenzimplementierung)

### 2.1 Zwei-Stufen-Prozess

**Stufe 1: Großjustierung (Landmark-basiert)**
- Benutzer wählt ≥3 korrespondierende Punkte in beiden Bildern
- Berechnung nach Kanatani (IEEE PAMI 1987)

**Stufe 2: Feinjustierung (ICP)**
- Vollständig iterativ, automatisch
- Implementierung in `ICPAlgorithm2014.java`

### 2.2 Kanatani-SVD (Großjustierung)

```
Eingabe: n Punktepaare (p_i, q_i), p aus Baseline, q aus Follow-up

1. Schwerpunkte: p̄ = Σp_i/n, q̄ = Σq_i/n

2. Korrelationsmatrix K (3×3):
   K = Σ (p_i - p̄)(q_i - q̄)^T

3. SVD: K = U · S · V^T

4. Rotation mit Vorzeichenkorrektur:
   R = U · diag(1, 1, det(U·V^T)) · V^T

5. Translation:
   T = p̄ - R · q̄
```

Determinanten-Korrektur verhindert Spiegelung statt Rotation.

### 2.3 ICP-Parameter (Outlier-Modi)

| Parameter | Bedeutung |
|-----------|-----------|
| `refine_clamp` | Entferne Punkte mit \|d\| > k·σ + µ |
| `refine_sd` | Entferne Punkte mit \|d\| > k·σ |
| `refine_clip` | Behalte nur beste p% nach Distanz |
| `refine_sparse` | Random-Subsampling (Anteil p) |
| `refine_unique` | 1-zu-1 Mapping (kein Modellpunkt doppelt) |
| `minimum_valid_points` | Abbruch wenn zu wenige Punkte |

Konvergenzkriterien:
- `ERROR_BOUND = 0.0001` (absoluter RMS)
- `ERROR_DIFF = 0.00001` (relativer RMS-Unterschied)

### 2.4 ICP-Hauptschleife

```
Iteration k:
1. Transformierte Punkte berechnen:
   w_i = R_k · d_i + T_k

2. Nächste Nachbarn im KD-Baum:
   c_i = arg min_j ||w_i - m_j||

3. Outlier entfernen (je nach Modus)

4. Kovarianzmatrix K aus verbleibenden Paaren

5. SVD → neue Rotation R_{k+1}

6. Translation T_{k+1} = centroid_model - R_{k+1} · centroid_data

7. RMS berechnen, Konvergenztest
```

---

## 3. Neugebauer 1991 (Historische Referenz)

### 3.1 MATCH3D.C – Ablauf

1. Beide Tiefenbilder laden
2. Optional: Benutzer klickt korrespondierende Punkte (max. 128 Paare)
3. Manuelle Eingabe von Rotations-Startwinkeln und Translation
4. `adjust3d()`: Optimierung der Registrierungsparameter
5. `differences()`: Differenzbild berechnen
6. Statistik: Stddev, Min/Max, Quantile

### 3.2 GROBJUST.C – `comp_location()`

```
Eingabe: PIXEL pixellist[2*n] (abwechselnd Baseline/Follow-up)

Phase 1: Akkumulationsmatrix B_mat[4][4]
  FOR j=1..n, i=0..j-1:
    pat_vec = pixellist[2i] - pixellist[2j]   (Baseline-Differenzvektor)
    mod_vec = pixellist[2i+1] - pixellist[2j+1] (Follow-up-Differenzvektor)
    Normieren
    Dyadisches Produkt berechnen
    B_mat akkumulieren (symmetrisch)

Phase 2: Kleinsten Eigenvektor von B_mat
  → Methode: Cholesky + Inverse Iteration
  → Quaternion der optimalen Rotation

Phase 3: Quaternion → Eulerwinkel (rx, ry, rz)

Phase 4: Translation (Mittelwert über alle Paare)
```

### 3.3 Eigenvektor-Methode (`low_eigenvector`)

```
1. Cholesky-Faktorisierung: B = L·L^T
2. Inverse Iteration bis ε < 1e-6:
   - Normalisieren
   - Löse L·L^T · v_neu = v_alt
   - Konvergenztest: |maxEigen - minEigen| / |maxEigen + minEigen| < ε
3. Exakter Eigenvektor und -wert
```

---

## 4. Vergleich der Strategien

| Kriterium | CCCoreLib ICP | Kunzelmann ICP | Neugebauer |
|-----------|---------------|----------------|------------|
| Automatisierung | Vollautomatisch | Semi-automatisch | Manuell |
| Großjustierung | Optional (FPCSReg.) | Kanatani SVD (Pflicht) | Eigenvektor (direkt) |
| Feinjustierung | ICP iterativ | ICP iterativ | Nein |
| Outlier-Behandlung | σ-Test | 5 Modi | Keine |
| Skalierung | Optional | Nein | Nein |
| Parallelisierung | Ja (Qt Threads) | Nein | Nein |
| Partial Overlap | Ja | Nein | Nein |
| Abhängigkeiten | CCCoreLib | JAMA (Java) | Keine |
| Komplexität | Hoch (ausgereift) | Mittel | Gering |
| Rotationsdarst. | Matrix + Quaternion | Matrix 3×3 | Eulerwinkel |
| Ausgabe | ScaledTransformation | 4×4-Matrix | 6 Winkel+Transl. |

### Empfehlung für match3d_v2

| Schritt | Methode | Begründung |
|---------|---------|------------|
| Großjustierung | Manuelle Punkte + Kanatani SVD | Wie Original, bewährt für Zahndaten |
| Feinjustierung | CCCoreLib ICP | Ausgereift, Qt-kompatibel, erweiterbar |
| Fallback | Kunzelmann-ICP (C++ portiert) | Falls CCCoreLib-Integration Probleme macht |

---

## 5. Aktuelle Implementierung in match3d_v2

### 5.1 Verfügbare Registrierungs-Methoden

| Button | Methode | DOF | Implementierung |
|--------|---------|-----|-----------------|
| **From COM** | Schwerpunkt-Translation | 3 | `CoarseRegistration::fromCOM()` |
| **From Points** | Landmark-basiert (Kanatani) | 4 | `CoarseRegistration::fromPoints()` |
| **Align** | Custom 2.5D ICP | 4 | `RegistrationWorker::run4DOF()` |
| **Refine** | Point-to-Plane ICP (Neugebauer) | 6 | `RegistrationWorker::run6DOF()` |
| **ICP** | CCCoreLib ICP (Besl & McKay) | 6 | `RegistrationWorker::runCCLibICP()` |

### 5.2 Align (4-DOF): Custom 2.5D ICP

Speziell für Tiefenbilder (Heightmaps) entwickelt, da Standard-3D-ICP versagt:

**Problem:** Z-Werte sind Größenordnungen größer als X,Y. Nächste-Nachbar-Suche in 3D findet falsche Korrespondenzen.

**Lösung:** 2.5D-Ansatz
1. Korrespondenzen über (X,Y)-Gitterposition finden (nicht 3D-Distanz)
2. Rotation nur um Z-Achse (alpha)
3. Translation in allen drei Achsen (tx, ty, tz)
4. Z-Offset als Median berechnen (robust gegen Ausreißer)

```cpp
// Inverse Transform: Model → Data Koordinaten
const double dxWorld = (mx - T.tx) * cosA + (my - T.ty) * sinA;
const double dyWorld = -(mx - T.tx) * sinA + (my - T.ty) * cosA;

// Bilineare Interpolation im Data-Bild
// Z-Differenz → Median für robuste Schätzung
```

### 5.3 Refine (6-DOF): Point-to-Plane ICP

Implementiert den Neugebauer-Algorithmus (1991) mit Punkt-zu-Ebene-Distanz:

**Grundidee:** Minimiere `n · (P' - Q)` statt `||P' - Q||`
- `Q`: Model-Punkt mit Oberflächennormale `n`
- `P'`: Transformierter Data-Punkt

**Jacobian:** Für kleine Winkeländerungen linearisiert:
```
J = [(P × n), n]  für Parameter (γ, β, α, tx, ty, tz)
```

**Normale aus Gradienten:**
```cpp
n = (-dz/dx, -dz/dy, 1) / ||...||
// dz/dx via zentrale Differenzen
```

**Schlüssel-Entscheidungen:**
- Slope-Filter: Nur Flächen < 89° Steigung verwenden
- Outlier-Rejection: |residual| > 3 × RMS
- Cholesky-Löser für 6×6 Normalgleichungssystem
- Dämpfung bei zu großen Updates (Divergenzschutz)

### 5.4 ICP (CCCoreLib): Standard Besl & McKay

Nutzt die ausgereifte CCCoreLib-Implementierung:

```cpp
CCCoreLib::ICPRegistrationTools::Parameters params;
params.convType = MAX_ITER_CONVERGENCE;
params.nbMaxIterations = cfg_.maxIterations;
params.filterOutFarthestPoints = true;  // Outlier-Filter
params.samplingLimit = cfg_.samplingLimit;
params.finalOverlapRatio = cfg_.overlapRatio;
params.adjustScale = false;  // Keine Skalierung für Zahnscans
params.transformationFilters = SKIP_NONE;  // Volle 6-DOF

CCCoreLib::ICPRegistrationTools::Register(
    &modelCloud, nullptr, &dataCloud, params,
    finalTrans, finalRMS, finalPointCount, this);
```

**Workflow:**
1. Initiale Transformation auf Data-Cloud anwenden
2. CCCoreLib ICP verfeinert die Ausrichtung
3. Euler-Winkel aus Rotationsmatrix extrahieren

**Vorteile gegenüber Custom-ICP:**
- KD-Baum für effiziente Nachbarsuche
- Robuste Outlier-Behandlung (σ-Test)
- Multi-Threading-Support
- Partial-Overlap-Handling

### 5.5 Typischer Workflow

```
1. From COM oder From Points  → Grobe Ausrichtung
2. Align (4-DOF)              → Rotation + Translation verfeinern
   ODER
   ICP (CCCoreLib)            → Alternative zu Align
3. Refine (6-DOF)             → Feinste Ausrichtung (optional)
4. Diff.img.                  → Differenzbild berechnen
```

**Statistik-Vergleich (Beispieldaten):**

| Methode | StdDev | Min | Max | Valid Pixels |
|---------|--------|-----|-----|--------------|
| From Points | 0.92 mm | -5.6 | +5.4 | 501k |
| Align | 1.79 mm | -8.1 | +7.0 | 444k |
| Refine | **0.66 mm** | -3.0 | +5.0 | 489k |

→ Refine liefert das beste Ergebnis. Align kann bei schwierigen Daten divergieren.

---

## 6. Software-Design: Strategy-Pattern (Konzept)

### 6.1 Abstrakte Basis

```cpp
class IRegistrationStrategy {
public:
    virtual ~IRegistrationStrategy() = default;

    // Konfigurieren
    virtual void setParameters(const RegistrationParameters& p) = 0;

    // Großjustierung mit manuellen Punktepaaren
    virtual bool coarseRegister(
        const std::vector<CCVector3>& landmarksModel,
        const std::vector<CCVector3>& landmarksData,
        ScaledTransformation& outTrans) = 0;

    // Feinregistrierung (ICP oder Direktmethode)
    virtual CCCoreLib::ICPRegistrationTools::RESULT_TYPE fineRegister(
        CCCoreLib::GenericIndexedCloudPersist* modelCloud,
        CCCoreLib::GenericIndexedCloudPersist* dataCloud,
        ScaledTransformation& inOutTrans,
        double& outRMS,
        unsigned& outPointCount) = 0;

    // Fortschritt-Callback
    using ProgressCallback = std::function<void(int iteration, double rms)>;
    virtual void setProgressCallback(ProgressCallback cb) = 0;

    // Name für GUI
    virtual QString strategyName() const = 0;
};
```

### 6.2 Konkreter Strategy: CCCoreLib ICP

```cpp
class CCCoreLibICPStrategy : public IRegistrationStrategy {
public:
    void setParameters(const RegistrationParameters& p) override {
        icpParams_.convType              = p.convergenceType;
        icpParams_.minRMSDecrease        = p.minRMSDecrease;
        icpParams_.nbMaxIterations       = p.maxIterations;
        icpParams_.filterOutFarthestPoints = p.filterOutliers;
        icpParams_.samplingLimit         = p.samplingLimit;
        icpParams_.finalOverlapRatio     = p.overlapRatio;
        icpParams_.adjustScale           = p.estimateScale;
    }

    bool coarseRegister(
        const std::vector<CCVector3>& lm,
        const std::vector<CCVector3>& ld,
        ScaledTransformation& outTrans) override
    {
        // Horn-Methode für korrespondierende Punkte
        CCCoreLib::PointCloud modelLM, dataLM;
        for (auto& p : lm) modelLM.addPoint(p);
        for (auto& p : ld) dataLM.addPoint(p);
        return CCCoreLib::HornRegistrationTools::FindAbsoluteOrientation(
            &modelLM, &dataLM, outTrans, true);
    }

    CCCoreLib::ICPRegistrationTools::RESULT_TYPE fineRegister(
        CCCoreLib::GenericIndexedCloudPersist* model,
        CCCoreLib::GenericIndexedCloudPersist* data,
        ScaledTransformation& inOutTrans,
        double& outRMS,
        unsigned& outCount) override
    {
        return CCCoreLib::ICPRegistrationTools::Register(
            model, nullptr, data, icpParams_,
            false, inOutTrans, outRMS, outCount);
    }

    QString strategyName() const override { return "ICP (CCCoreLib)"; }

private:
    CCCoreLib::ICPRegistrationTools::Parameters icpParams_;
};
```

### 6.3 Parameter-Struktur

```cpp
struct RegistrationParameters {
    // Konvergenz
    CCCoreLib::ICPRegistrationTools::CONVERGENCE_TYPE convergenceType
        = CCCoreLib::ICPRegistrationTools::MAX_ERROR_CONVERGENCE;
    double   minRMSDecrease = 1.0e-5;
    unsigned maxIterations  = 20;

    // Outlier
    bool   filterOutliers = false;
    double outlierSigmaFactor = 2.5;

    // Datenreduktion
    unsigned samplingLimit = 50000;
    double   overlapRatio  = 1.0;

    // Skalierung
    bool   estimateScale = false;

    // Grobjustierung
    bool   useCoarseRegistration = true;

    // Vordefinierte Profile
    static RegistrationParameters dentalWearDefault();
    static RegistrationParameters highPrecision();
    static RegistrationParameters partialOverlap();
};

RegistrationParameters RegistrationParameters::dentalWearDefault() {
    RegistrationParameters p;
    p.convergenceType = CCCoreLib::ICPRegistrationTools::MAX_ERROR_CONVERGENCE;
    p.minRMSDecrease  = 1.0e-5;
    p.maxIterations   = 20;
    p.filterOutliers  = true;
    p.outlierSigmaFactor = 2.5;
    p.samplingLimit   = 50000;
    p.overlapRatio    = 1.0;
    p.useCoarseRegistration = true;
    return p;
}
```

### 6.4 Factory

```cpp
class RegistrationStrategyFactory {
public:
    enum class Type { CCCoreLib_ICP };

    static std::unique_ptr<IRegistrationStrategy> create(Type type) {
        switch (type) {
            case Type::CCCoreLib_ICP:
                return std::make_unique<CCCoreLibICPStrategy>();
        }
        return nullptr;
    }

    // Für GUI: verfügbare Strategien
    static QStringList availableStrategies() {
        return { "ICP (CCCoreLib)" };
    }
};
```

### 6.5 Orchestrierung (Match3DRegistration)

```cpp
class Match3DRegistration : public QObject {
    Q_OBJECT
public:
    // Aufruf aus GUI
    void startRegistration(
        const ViffReader::DepthImage& model,
        const ViffReader::DepthImage& data,
        const std::vector<CCVector3>& landmarksModel,
        const std::vector<CCVector3>& landmarksData,
        const RegistrationParameters& params);

signals:
    void progressUpdated(int iteration, double rms);
    void registrationFinished(ScaledTransformation trans, double finalRMS);
    void registrationFailed(QString error);

private slots:
    void onWorkerFinished(ScaledTransformation trans, double rms, QString error);

private:
    std::unique_ptr<IRegistrationStrategy> strategy_;
    QThread* workerThread_ = nullptr;
};
```

---

## 7. Workflow: Gesamtprozess

```
1. Beide VIFF-Bilder laden (ViffReader)
       ↓
2. In Punktwolken konvertieren
   (gültige Pixel → CCVector3 mit Weltkoordinaten)
       ↓
3. [Optional] Großjustierung
   a. Benutzer wählt ≥3 Punkte in Bild 1
   b. Benutzer wählt ≥3 Punkte in Bild 2
   c. HornRegistration → initiale Transformation
       ↓
4. Feinjustierung: ICP
   - dataCloud mit initialer Transformation vorausrichten
   - ICPRegistrationTools::Register()
   - Fortschritt → GUI-Update via Signal/Slot
       ↓
5. Transformation anwenden
       ↓
6. Differenzbild berechnen
   - Für jeden Pixel (x,y) in Bild 1:
     Projiziere Bild-2-Punkt mit Transformation
     Berechne Z-Differenz durch bilineare Interpolation
       ↓
7. Anzeige + Statistik
   - Differenzbild als Falschfarben oder Grauwert
   - Min/Max/Stddev der Differenzen
       ↓
8. Speichern
   - Transformation als Text
   - Differenzbild als VIFF
```
