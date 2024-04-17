//
//  ShareTableViewDataSource.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 27/2/24.
//

import AppKit
import FileProvider
import NextcloudKit
import OSLog

class ShareTableViewDataSource: NSObject, NSTableViewDataSource {
    var sharesTableView: NSTableView? {
        didSet {
            sharesTableView?.dataSource = self
            sharesTableView?.reloadData()
        }
    }
    private var itemIdentifier: NSFileProviderItemIdentifier?
    private var itemURL: URL?
    private var shares: [NKShare] = [] {
        didSet { sharesTableView?.reloadData() }
    }

    func loadItem(identifier: NSFileProviderItemIdentifier, url: URL) {
        itemIdentifier = identifier
        itemURL = url
        Task {
            await reload()
        }
    }

    private func reload() async {
        guard let itemIdentifier = itemIdentifier, let itemURL = itemURL else { return }
        do {
            let connection = try await serviceConnection(url: itemURL)
            shares = await connection.shares(forItemIdentifier: itemIdentifier) ?? []
        } catch let error {
            Logger.sharesDataSource.error("Could not reload data: \(error)")
        }
    }

    private func serviceConnection(url: URL) async throws -> FPUIExtensionService {
        let services = try await FileManager().fileProviderServicesForItem(at: url)
        guard let service = services[fpUiExtensionServiceName] else {
            Logger.sharesDataSource.error("Couldn't get service, required service not present")
            throw NSFileProviderError(.providerNotFound)
        }
        let connection: NSXPCConnection
        connection = try await service.fileProviderConnection()
        connection.remoteObjectInterface = NSXPCInterface(with: FPUIExtensionService.self)
        connection.interruptionHandler = {
            Logger.sharesDataSource.error("Service connection interrupted")
        }
        connection.resume()
        guard let proxy = connection.remoteObjectProxy as? FPUIExtensionService else {
            throw NSFileProviderError(.serverUnreachable)
        }
        return proxy
    }
}
