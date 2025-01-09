VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
  Persistable = 0  'NotPersistable
  DataBindingBehavior = 0  'vbNone
  DataSourceBehavior  = 0  'vbNone
  MTSTransactionMode  = 0  'NotAnMTSObject
END
Attribute VB_Name = "clsMigemo"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = True
Attribute VB_PredeclaredId = False
Attribute VB_Exposed = False
Attribute VB_Ext_KEY = "SavedWithClassBuilder6" ,"Yes"
Attribute VB_Ext_KEY = "Top_Level" ,"Yes"
Option Explicit

Public Enum DictionaryId
    enmInvalid = 0
    enmMigemo = 1
    enmRomaToHira = 2
    enmHiraToKata = 3
    enmHanToZen = 4
    enmZenToHan = 5
End Enum

Public Enum OperatorIndex
    enmOr = 0
    enmNestIn = 1
    enmNestOut = 2
    enmSelectIn = 3
    enmSelectOut = 4
    enmNewLine = 5
End Enum

Private Const enmDictMigemo = "migemo-dict"
Private Const enmDictRoma2Hira = "roma2hira.dat"
Private Const enmDictHira2Kata = "hira2kata.dat"
Private Const enmDictHan2Zen = "han2zen.dat"
Private Const enmDictZen2Han = "zen2han.dat"

Private Declare Function migemo_open Lib "migemo.dll" (ByVal strDict As String) As Long
Private Declare Sub migemo_close Lib "migemo.dll" (ByVal hMigemo As Long)
Private Declare Function migemo_query Lib "migemo.dll" (ByVal hMigemo As Long, ByVal strQuery As String) As Long
Private Declare Sub migemo_release Lib "migemo.dll" (ByVal hMigemo As Long, ByVal pAnswer As Long)
Private Declare Function migemo_set_operator Lib "migemo.dll" (ByVal hMigemo As Long, ByVal EnmIndex As OperatorIndex, ByVal strOp As String) As Long
Private Declare Function migemo_get_operator Lib "migemo.dll" (ByVal hMigemo As Long, ByVal EnmIndex As OperatorIndex) As Long
Private Declare Function migemo_load Lib "migemo.dll" (ByVal hMigemo As Long, ByVal EnmDict As DictionaryId, ByVal strFile As String) As DictionaryId
Private Declare Function migemo_is_enable Lib "migemo.dll" (ByVal hMigemo As Long) As Long
Private Declare Sub CopyMemory Lib "kernel32.dll" Alias "RtlMoveMemory" (pTo As Any, uFrom As Any, ByVal lSize As Long)
Private Declare Function StrLen Lib "kernel32.dll" Alias "lstrlen" (ByVal Ptr As Long) As Long

Private m_hMigemo As Long

Private Function PtrStr(lpString As Long) As String
  Dim buff() As Byte
  Dim nSize As Long
  
  If lpString Then
    nSize = StrLen(lpString)
    If nSize Then
      ReDim buff(0 To (nSize - 1)) As Byte
      CopyMemory buff(0), ByVal lpString, nSize
      PtrStr = buff
    End If
  End If
End Function

Private Sub Class_Initialize()
    m_hMigemo = migemo_open(0)
    OperatorNestIn = "(?:"
End Sub

Private Sub Class_Terminate()
    If m_hMigemo Then
        Call migemo_close(m_hMigemo)
        m_hMigemo = 0
    End If
End Sub

Public Function SetOperator(EnmIndex As OperatorIndex, strOp As String) As Boolean
    SetOperator = migemo_set_operator(m_hMigemo, EnmIndex, strOp)
End Function

Public Function GetOperator(EnmIndex As OperatorIndex) As String
    GetOperator = StrConv(PtrStr(migemo_get_operator(m_hMigemo, EnmIndex)), vbUnicode)
End Function

Public Property Let OperatorOr(ByVal strOp As String)
    Call SetOperator(enmOr, strOp)
End Property

Public Property Get OperatorOr() As String
    OperatorOr = GetOperator(enmOr)
End Property

Public Property Let OperatorNestIn(ByVal strOp As String)
    Call SetOperator(enmNestIn, strOp)
End Property

Public Property Get OperatorNestIn() As String
    OperatorNestIn = GetOperator(enmNestIn)
End Property

Public Property Let OperatorNestOut(ByVal strOp As String)
    Call SetOperator(enmNestOut, strOp)
End Property

Public Property Get OperatorNestOut() As String
    OperatorNestOut = GetOperator(enmNestOut)
End Property

Public Property Let OperatorSelectIn(ByVal strOp As String)
    Call SetOperator(enmSelectIn, strOp)
End Property

Public Property Get OperatorSelectIn() As String
    OperatorSelectIn = GetOperator(enmSelectIn)
End Property

Public Property Let OperatorSelectOut(ByVal strOp As String)
    Call SetOperator(enmSelectOut, strOp)
End Property

Public Property Get OperatorSelectOut() As String
    OperatorSelectOut = GetOperator(enmSelectOut)
End Property

Public Property Let OperatorNewLine(ByVal strOp As String)
    Call SetOperator(enmNewLine, strOp)
End Property

Public Property Get OperatorNewLine() As String
    OperatorNewLine = GetOperator(enmNewLine)
End Property

Public Function LoadDictionary(EnmDictId As DictionaryId, strFile As String) As Boolean
    LoadDictionary = False
    If migemo_load(m_hMigemo, EnmDictId, strFile) = EnmDictId Then
        LoadDictionary = True
    End If
End Function

Public Function LoadDictionaryAll(strFile As String) As Boolean
    Dim strBase As String
    Dim lngLoaded As Long
    LoadDictionaryAll = False
    
    strBase = Left(strFile, InStrRev(strFile, "/"))
    If Len(strBase) = 0 Then
        strBase = Left(strFile, InStrRev(strFile, "\"))
    End If
    
    lngLoaded = 0
    lngLoaded = lngLoaded + IIf(LoadDictionary(enmMigemo, strFile), 1, 0)
    lngLoaded = lngLoaded + IIf(LoadDictionary(enmRomaToHira, strBase & enmDictRoma2Hira), 1, 0)
    lngLoaded = lngLoaded + IIf(LoadDictionary(enmHiraToKata, strBase & enmDictHira2Kata), 1, 0)
    lngLoaded = lngLoaded + IIf(LoadDictionary(enmHanToZen, strBase & enmDictHan2Zen), 1, 0)
    lngLoaded = lngLoaded + IIf(LoadDictionary(enmZenToHan, strBase & enmDictZen2Han), 1, 0)
    If lngLoaded = 5 Then
        LoadDictionaryAll = True
    End If
End Function

Public Function IsEnable() As Boolean
    IsEnable = migemo_is_enable(m_hMigemo)
End Function

Public Function GetRegex(strQuery As String) As Object
    Set GetRegex = CreateObject("VBScript.RegExp")
    GetRegex.Pattern = Query(strQuery)
End Function

Public Function Query(strQuery As String) As String
    Dim pAnswer As Long
    pAnswer = migemo_query(m_hMigemo, strQuery)
    If pAnswer Then
        Query = StrConv(PtrStr(pAnswer), vbUnicode)
        Call migemo_release(m_hMigemo, pAnswer)
    Else
        Query = ""
    End If
End Function

Public Sub Dispose()
    If m_hMigemo Then
        Call migemo_close(m_hMigemo)
        m_hMigemo = 0
    End If
End Sub

