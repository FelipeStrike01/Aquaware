# AquaWare Driver

Driver de kernel para Windows desenvolvido com KMDF (Kernel Mode Driver Framework).

## 📋 Descrição

O AquaWare é um driver de sistema para Windows que fornece funcionalidades de gerenciamento e comunicação em modo kernel. O projeto suporta múltiplas arquiteturas e é compilado usando o Visual Studio com ferramentas nativas do Windows.

## 🏗️ Estrutura do Projeto

```
aquaware/
├── include/              # Arquivos de cabeçalho
│   ├── common/          # Tipos e definições comuns
│   ├── core/            # Logging, estado e validação
│   ├── io/              # Manipuladores IOCTL
│   └── process/         # Utilitários de processo
├── src/                 # Código-fonte
│   ├── main.cpp         # Ponto de entrada do driver
│   ├── core/            # Implementações de logging, estado e validação
│   ├── io/              # Implementações de IOCTL
│   └── process/         # Implementações de utilitários
├── aquaware.inf         # Arquivo INF do driver
└── AquaWare.vcxproj     # Projeto Visual Studio
```

## 🛠️ Requisitos

- **Windows 10** ou superior
- **Visual Studio 2022** com suporte a driver de kernel
- **Windows Driver Kit (WDK)** para Windows 10
- **.NET Framework** 4.5+

## 📦 Compilação

1. Abra `Driver.sln` no Visual Studio
2. Selecione a configuração desejada:
   - **Debug|x64** - Compilação de debug para 64 bits
   - **Release|x64** - Compilação de release para 64 bits
   - **Debug|ARM64** - Compilação de debug para ARM64
   - **Release|ARM64** - Compilação de release para ARM64
3. Compile o projeto (Ctrl+Shift+B)

Os artefatos compilados serão gerados na pasta `build/`.

## 📚 Módulos Principais

- **Logging** - Sistema de logs para rastreamento e debug
- **State** - Gerenciamento de estado do driver
- **Validation** - Validação de dados e requisições
- **IOCTL Handlers** - Manipuladores de controle de entrada/saída
- **Process Utils** - Utilitários para gerenciamento de processos

## ⚙️ Configuração

As configurações principais são definidas em:
- `main.cpp` - Nomes de dispositivo e links simbólicos
- `aquaware.inf` - Informações de instalação do driver

## 📝 Licença

Este projeto é fornecido como está. Verifique os termos de uso específicos.

## ✉️ Suporte

Para dúvidas ou relatório de problemas, verifique a documentação do projeto ou entre em contato com o mantenedor.
